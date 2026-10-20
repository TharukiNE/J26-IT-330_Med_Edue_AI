from dataclasses import dataclass

from component3.src.knowledge_graph import CurriculumKnowledgeGraph
from component3.src.mastery import LearnerMasteryProfile


@dataclass(frozen=True)
class WeakAreaAnalysis:
    weak_topics: tuple[str, ...]
    missing_prerequisites: dict[str, tuple[str, ...]]


def analyze_weak_areas(
    profile: LearnerMasteryProfile,
    knowledge_graph: CurriculumKnowledgeGraph,
    low_mastery_threshold: float = 0.5,
) -> WeakAreaAnalysis:
    """Find weak topics and their direct prerequisites not yet mastered."""
    if not isinstance(profile, LearnerMasteryProfile):
        raise TypeError("profile must be a LearnerMasteryProfile")
    if not isinstance(knowledge_graph, CurriculumKnowledgeGraph):
        raise TypeError("knowledge_graph must be a CurriculumKnowledgeGraph")
    if (
        isinstance(low_mastery_threshold, bool)
        or not isinstance(low_mastery_threshold, (int, float))
    ):
        raise TypeError("low_mastery_threshold must be a number")
    if not 0.0 <= low_mastery_threshold <= 1.0:
        raise ValueError("low_mastery_threshold must be between 0.0 and 1.0")

    weak_topics = tuple(
        sorted(
            topic
            for topic, mastery in profile.topics.items()
            if mastery.mastery_score < low_mastery_threshold
        )
    )
    missing_prerequisites = {
        topic: tuple(
            prerequisite
            for prerequisite in knowledge_graph.get_prerequisites(topic)
            if prerequisite not in profile.topics
            or profile.topics[prerequisite].mastery_score < low_mastery_threshold
        )
        for topic in weak_topics
    }
    return WeakAreaAnalysis(
        weak_topics=weak_topics,
        missing_prerequisites=missing_prerequisites,
    )
