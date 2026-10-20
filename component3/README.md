# Component 3 – Explainable Personalized Recommendation System

Initial project structure for the explainable personalized recommendation system.

## Learner mastery profile

The `src/mastery` module accepts `LearnerPerformance` records containing a topic
and whether the learner answered correctly. `build_learner_mastery_profile`
aggregates attempts by topic and calculates mastery as the proportion of correct
answers. The profile is stored in memory and can be returned as a dictionary
with `to_dict()`. This is a basic profile structure only; it does not implement
recommendations or claim research-based mastery outcomes.

Run its tests from the repository root with:

```powershell
python -m unittest component3.tests.test_mastery_profile
```

## Curriculum knowledge graph

The `src/knowledge_graph` module stores medical topics as nodes and records
direct prerequisite relationships. Adding a prerequisite also adds both topics
to the graph. `get_prerequisites()` returns the direct prerequisites for a
known topic as a sorted tuple. This is a basic in-memory graph structure, not a
complete curriculum or recommendation system.

Run its tests from the repository root with:

```powershell
python -m unittest component3.tests.test_knowledge_graph
```

## Weak-area analysis

The `src/analysis` module uses a learner mastery profile and the curriculum
knowledge graph to report topics with mastery below `0.5` and their direct
prerequisites that are below the same threshold or have no mastery record.
The threshold can be configured. This is a basic analysis only; it does not
provide recommendations.

Run its tests from the repository root with:

```powershell
python -m unittest component3.tests.test_weak_area_analysis
```
