import unittest

from component3.src.mastery import (
    LearnerMasteryProfile,
    LearnerPerformance,
    build_learner_mastery_profile,
    calculate_mastery_score,
)


class CalculateMasteryScoreTests(unittest.TestCase):
    def test_returns_accuracy_ratio(self) -> None:
        self.assertEqual(calculate_mastery_score(3, 4), 0.75)

    def test_returns_zero_without_attempts(self) -> None:
        self.assertEqual(calculate_mastery_score(0, 0), 0.0)

    def test_rejects_invalid_counts(self) -> None:
        with self.assertRaises(ValueError):
            calculate_mastery_score(2, 1)


class LearnerMasteryProfileTests(unittest.TestCase):
    def test_builds_topic_level_profile_from_performance_records(self) -> None:
        profile = build_learner_mastery_profile(
            "learner-1",
            [
                LearnerPerformance(topic="fractions", correct=True),
                LearnerPerformance(topic="fractions", correct=False),
                LearnerPerformance(topic="geometry", correct=True),
            ],
        )

        self.assertEqual(
            profile.to_dict(),
            {
                "learner_id": "learner-1",
                "topics": {
                    "fractions": {
                        "attempts": 2,
                        "correct_answers": 1,
                        "mastery_score": 0.5,
                    },
                    "geometry": {
                        "attempts": 1,
                        "correct_answers": 1,
                        "mastery_score": 1.0,
                    },
                },
            },
        )

    def test_profile_can_accumulate_performance_after_creation(self) -> None:
        profile = LearnerMasteryProfile(learner_id="learner-2")

        profile.record_performance(LearnerPerformance(topic="algebra", correct=True))

        self.assertEqual(profile.to_dict()["topics"]["algebra"]["mastery_score"], 1.0)

    def test_rejects_empty_topic(self) -> None:
        profile = LearnerMasteryProfile(learner_id="learner-3")

        with self.assertRaises(ValueError):
            profile.record_performance(LearnerPerformance(topic=" ", correct=True))


if __name__ == "__main__":
    unittest.main()
