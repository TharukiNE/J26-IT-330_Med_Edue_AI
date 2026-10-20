"""Stress model service for MedEduAI.
POST /train  {"csv_path":".../stress_detection.csv"}
POST /predict {"readings":[...]}  -> score, level, factors
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json, os, pickle
import numpy as np

MODEL_PATH = Path(os.getenv('STRESS_MODEL_PATH', Path(__file__).parent / 'stress_model.pkl'))
MODEL = None
FEATURES = ['sleep_duration', 'PSQI_score', 'screen_on_time', 'call_duration', 'num_calls', 'num_sms', 'skin_conductance', 'accelerometer', 'mobility_radius', 'mobility_distance', 'heartRate', 'spo2', 'bodyTemperature', 'roomTemperature', 'humidity', 'lightLux']

def load_model():
    global MODEL
    if MODEL_PATH.exists():
        with MODEL_PATH.open('rb') as stream: MODEL = pickle.load(stream)

def vector(row): return [float(row.get(name, np.nan)) if row.get(name) not in (None, '') else np.nan for name in FEATURES]
def impute(matrix, means):
    matrix = np.asarray(matrix, dtype=float)
    return np.where(np.isnan(matrix), means, matrix)

def train(csv_path):
    try:
        import pandas as pd
        from sklearn.ensemble import RandomForestRegressor, RandomForestClassifier
        from sklearn.impute import SimpleImputer
        from sklearn.metrics import mean_absolute_error, mean_squared_error, accuracy_score, f1_score
        from sklearn.model_selection import train_test_split
    except ImportError as error: raise RuntimeError(f'Install pandas, numpy, and scikit-learn: {error}')
    data = pd.read_csv(csv_path)
    target = 'PSS_score'
    missing = [name for name in FEATURES if name not in data.columns]
    usable = [name for name in FEATURES if name in data.columns]
    if target not in data.columns: raise RuntimeError('Dataset must contain PSS_score.')
    x = data[usable].apply(pd.to_numeric, errors='coerce')
    y = pd.to_numeric(data[target], errors='coerce').clip(10, 40)
    valid = y.notna(); x, y = x[valid], y[valid]
    imputer = SimpleImputer(strategy='median'); matrix = imputer.fit_transform(x)
    x_train, x_test, y_train, y_test = train_test_split(matrix, y, test_size=.2, random_state=42)
    reg = RandomForestRegressor(n_estimators=250, random_state=42, min_samples_leaf=3, n_jobs=-1); reg.fit(x_train, y_train)
    predicted = reg.predict(x_test)
    classifier = RandomForestClassifier(n_estimators=250, random_state=42, min_samples_leaf=3, n_jobs=-1); classifier.fit(x_train, (y_train >= 30).astype(int) + (y_train >= 20).astype(int))
    classes = classifier.predict(x_test)
    artifact = {'regressor': reg, 'classifier': classifier, 'imputer': imputer, 'features': usable, 'missing': missing}
    try:
        import shap
        artifact['shap_explainer'] = shap.TreeExplainer(reg)
    except Exception:
        artifact['shap_explainer'] = None
    with MODEL_PATH.open('wb') as stream: pickle.dump(artifact, stream)
    load_model()
    return {'rows': len(data), 'featuresUsed': usable, 'featuresMissing': missing, 'mae': float(mean_absolute_error(y_test, predicted)), 'rmse': float(mean_squared_error(y_test, predicted) ** .5), 'categoryF1': float(f1_score((y_test >= 30).astype(int) + (y_test >= 20).astype(int), classes, average='weighted'))}

def predict(readings):
    if MODEL is None: raise RuntimeError('Stress model is not trained. POST /train with the dataset first.')
    latest = readings[-100:]
    row = {key: np.nanmean([float(item[key]) for item in latest if item.get(key) not in (None, '')]) for key in FEATURES if any(item.get(key) not in (None, '') for item in latest)}
    matrix = [[row.get(name, np.nan) for name in MODEL['features']]]
    score = float(np.clip(MODEL['regressor'].predict(impute(matrix, MODEL['imputer'].statistics_))[0], 10, 40))
    level = 'low' if score < 20 else 'moderate' if score < 30 else 'high'
    values = matrix[0]
    explanations = []
    method = 'Random Forest feature-deviation explanation'
    if MODEL.get('shap_explainer') is not None:
        try:
            shap_values = MODEL['shap_explainer'].shap_values(np.asarray([values]))
            contributions = np.asarray(shap_values[0] if isinstance(shap_values, list) else shap_values).reshape(-1)
            explanations = [{'feature': name, 'impact': round(abs(float(contribution)), 3), 'direction': 'increasing risk' if contribution > 0 else 'reducing risk'} for name, contribution in zip(MODEL['features'], contributions)]
            explanations.sort(key=lambda item: item['impact'], reverse=True)
            method = 'SHAP TreeExplainer contributions'
        except Exception:
            pass
    if not explanations:
        factors = sorted(((name, float(value) - float(MODEL['imputer'].statistics_[index])) for index, (name, value) in enumerate(zip(MODEL['features'], values)) if not np.isnan(value)), key=lambda item: abs(item[1]), reverse=True)[:5]
        explanations = [{'feature': name, 'impact': round(abs(delta), 3), 'direction': 'increasing risk' if delta > 0 else 'reducing risk'} for name, delta in factors]
    return {'stressScore': round(score, 2), 'stressLevel': level, 'mainFactors': [item['feature'] for item in explanations[:3]], 'explanations': explanations[:5], 'windowSize': len(latest), 'confidence': round(min(0.99, .55 + min(len(latest), 100) / 250), 2), 'explainabilityMethod': method, 'medicalDisclaimer': 'Educational risk indicator only; not a medical diagnosis.'}

class Handler(BaseHTTPRequestHandler):
    def send_json(self, status, payload):
        body = json.dumps(payload).encode(); self.send_response(status); self.send_header('Content-Type', 'application/json'); self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
    def do_POST(self):
        try:
            body = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))))
            if self.path == '/train': return self.send_json(200, {'status': 'trained', **train(body['csv_path'])})
            if self.path == '/predict': return self.send_json(200, predict(body.get('readings', [])))
            self.send_json(404, {'error': 'Not found'})
        except Exception as error: self.send_json(400, {'error': str(error)})

load_model()
if __name__ == '__main__': ThreadingHTTPServer(('127.0.0.1', int(os.getenv('STRESS_PORT', '8020'))), Handler).serve_forever()
