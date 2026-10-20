import unittest

from component3.src.knowledge_graph import CurriculumKnowledgeGraph, TopicNode


class CurriculumKnowledgeGraphTests(unittest.TestCase):
    def test_adds_topics_and_retrieves_prerequisites(self) -> None:
        graph = CurriculumKnowledgeGraph()
        graph.add_topic("Cardiology")

        graph.add_prerequisite("Cardiology", "Anatomy")

        self.assertIsInstance(graph.topics["Cardiology"], TopicNode)
        self.assertIn("Anatomy", graph.topics)
        self.assertEqual(graph.get_prerequisites("Cardiology"), ("Anatomy",))
        self.assertEqual(graph.get_prerequisites("Anatomy"), ())

    def test_returns_prerequisites_in_sorted_order_without_duplicates(self) -> None:
        graph = CurriculumKnowledgeGraph()

        graph.add_prerequisite("Cardiology", "Physiology")
        graph.add_prerequisite("Cardiology", "Anatomy")
        graph.add_prerequisite("Cardiology", "Anatomy")

        self.assertEqual(
            graph.get_prerequisites("Cardiology"),
            ("Anatomy", "Physiology"),
        )

    def test_rejects_empty_topic(self) -> None:
        graph = CurriculumKnowledgeGraph()

        with self.assertRaises(ValueError):
            graph.add_topic(" ")

    def test_unknown_topic_raises_key_error(self) -> None:
        graph = CurriculumKnowledgeGraph()

        with self.assertRaises(KeyError):
            graph.get_prerequisites("Cardiology")


if __name__ == "__main__":
    unittest.main()
