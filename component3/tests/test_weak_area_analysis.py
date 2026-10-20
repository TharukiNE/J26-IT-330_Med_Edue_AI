import unittest

from component3.src.analysis import analyze_weak_areas
from component3.src.knowledge_graph import CurriculumKnowledgeGraph
from component3.src.mastery import LearnerMasteryProfile, LearnerPerformance


class AnalyzeWeakAreasTests(unittest.TestCase):
    def test_finds_weak_topics_and_low_mastery_prerequisites(self) -> None:
        profile = LearnerMasteryProfile("learner-1")
        profile.add_performances(
            [
                LearnerPerformance("cardiology", False),
                LearnerPerformance("anatomy", False),
                LearnerPerformance("physiology", True),
                LearnerPerformance("physiology", True),
            ]
        )
        graph = CurriculumKnowledgeGraph()
        graph.add_topic("anatomy")
        graph.add_prerequisite("cardiology", "physiology")
        graph.add_prerequisite("cardiology", "anatomy")

        result = analyze_weak_areas(profile, graph)

        self.assertEqual(result.weak_topics, ("anatomy", "cardiology"))
        self.assertEqual(
            result.missing_prerequisites,
            {"anatomy": (), "cardiology": ("anatomy",)},
        )

    def test_treats_unassessed_prerequisites_as_missing(self) -> None:
        profile = LearnerMasteryProfile("learner-2")
        profile.record_performance(LearnerPerformance("cardiology", False))
        graph = CurriculumKnowledgeGraph()
        graph.add_prerequisite("cardiology", "anatomy")

        result = analyze_weak_areas(profile, graph)

        self.assertEqual(result.weak_topics, ("cardiology",))
        self.assertEqual(
            result.missing_prerequisites,
            {"cardiology": ("anatomy",)},
        )

    def test_topics_at_threshold_are_not_weak(self) -> None:
        profile = LearnerMasteryProfile("learner-3")
        profile.add_performances(
            [
                LearnerPerformance("cardiology", False),
                LearnerPerformance("cardiology", True),
            ]
        )
        graph = CurriculumKnowledgeGraph()
        graph.add_topic("cardiology")

        result = analyze_weak_areas(profile, graph)

        self.assertEqual(result.weak_topics, ())
        self.assertEqual(result.missing_prerequisites, {})

    def test_rejects_threshold_outside_score_range(self) -> None:
        profile = LearnerMasteryProfile("learner-4")
        graph = CurriculumKnowledgeGraph()

        with self.assertRaises(ValueError):
            analyze_weak_areas(profile, graph, low_mastery_threshold=1.1)


if __name__ == "__main__":
    unittest.main()
