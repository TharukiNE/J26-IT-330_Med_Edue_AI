const mongoose = require('mongoose')

const sensorReadingSchema = new mongoose.Schema({
  deviceId: { type: String, required: true, index: true },
  studentId: { type: mongoose.Schema.Types.ObjectId, ref: 'KnowledgeNode', index: true },
  timestamp: { type: Date, required: true, index: true },
  heartRate: Number,
  spo2: Number,
  bodyTemperature: Number,
  roomTemperature: Number,
  humidity: Number,
  lightLux: Number,
  uptimeMs: Number,
}, { timestamps: true })

sensorReadingSchema.index({ deviceId: 1, timestamp: -1 })
module.exports = mongoose.model('SensorReading', sensorReadingSchema)
