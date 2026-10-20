from dataclasses import dataclass, field


@dataclass
class TopicNode:
    topic: str
    prerequisites: set[str] = field(default_factory=set)


class CurriculumKnowledgeGraph:
    """Store medical topics and their direct prerequisite relationships."""

    def __init__(self) -> None:
        self.topics: dict[str, TopicNode] = {}

    def add_topic(self, topic: str) -> TopicNode:
        name = self._normalize_topic(topic)
        return self.topics.setdefault(name, TopicNode(topic=name))

    def add_prerequisite(self, topic: str, prerequisite: str) -> None:
        topic_name = self._normalize_topic(topic)
        prerequisite_name = self._normalize_topic(prerequisite)
        topic_node = self.add_topic(topic_name)
        self.add_topic(prerequisite_name)
        topic_node.prerequisites.add(prerequisite_name)

    def get_prerequisites(self, topic: str) -> tuple[str, ...]:
        name = self._normalize_topic(topic)
        if name not in self.topics:
            raise KeyError(f"unknown topic: {name}")
        return tuple(sorted(self.topics[name].prerequisites))

    @staticmethod
    def _normalize_topic(topic: str) -> str:
        if not isinstance(topic, str):
            raise TypeError("topic must be a string")
        name = topic.strip()
        if not name:
            raise ValueError("topic must be a non-empty string")
        return name
