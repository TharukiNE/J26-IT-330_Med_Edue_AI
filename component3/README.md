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
