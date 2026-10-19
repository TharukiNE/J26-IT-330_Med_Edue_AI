from dataclasses import dataclass, field
from typing import Iterable


def calculate_mastery_score(correct_answers: int, attempts: int) -> float:
    """Return the proportion of correct answers, or 0.0 when there are no attempts."""
    if isinstance(correct_answers, bool) or not isinstance(correct_answers, int):
        raise TypeError("correct_answers must be an integer")
    if isinstance(attempts, bool) or not isinstance(attempts, int):
        raise TypeError("attempts must be an integer")
    if attempts < 0 or correct_answers < 0 or correct_answers > attempts:
        raise ValueError("answer counts must satisfy 0 <= correct_answers <= attempts")
    if attempts == 0:
        return 0.0
    return correct_answers / attempts


@dataclass(frozen=True)
class LearnerPerformance:
    topic: str
    correct: bool


@dataclass
class TopicMastery:
    attempts: int = 0
    correct_answers: int = 0

    @property
    def mastery_score(self) -> float:
        return calculate_mastery_score(self.correct_answers, self.attempts)

    def record(self, correct: bool) -> None:
        if not isinstance(correct, bool):
            raise TypeError("correct must be a boolean")
        self.attempts += 1
        if correct:
            self.correct_answers += 1

    def to_dict(self) -> dict[str, int | float]:
        return {
            "attempts": self.attempts,
            "correct_answers": self.correct_answers,
            "mastery_score": self.mastery_score,
        }


@dataclass
class LearnerMasteryProfile:
    learner_id: str
    topics: dict[str, TopicMastery] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if not isinstance(self.learner_id, str) or not self.learner_id.strip():
            raise ValueError("learner_id must be a non-empty string")

    def record_performance(self, performance: LearnerPerformance) -> None:
        if not isinstance(performance, LearnerPerformance):
            raise TypeError("performance must be a LearnerPerformance")
        if not isinstance(performance.topic, str):
            raise TypeError("topic must be a string")
        if not isinstance(performance.correct, bool):
            raise TypeError("correct must be a boolean")
        topic = performance.topic.strip()
        if not topic:
            raise ValueError("topic must be a non-empty string")
        self.topics.setdefault(topic, TopicMastery()).record(performance.correct)

    def add_performances(self, performances: Iterable[LearnerPerformance]) -> None:
        for performance in performances:
            self.record_performance(performance)

    def to_dict(self) -> dict[str, object]:
        return {
            "learner_id": self.learner_id,
            "topics": {
                topic: mastery.to_dict()
                for topic, mastery in sorted(self.topics.items())
            },
        }


def build_learner_mastery_profile(
    learner_id: str,
    performances: Iterable[LearnerPerformance],
) -> LearnerMasteryProfile:
    profile = LearnerMasteryProfile(learner_id=learner_id)
    profile.add_performances(performances)
    return profile
