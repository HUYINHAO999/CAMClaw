from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class FeatureRecognitionResult:
    operation_type: str
    confidence: float
    reason: str


class FeatureRecognizer:
    def recognize_operation_type(self, feature_text: str) -> FeatureRecognitionResult:
        normalized = feature_text.lower()
        if any(keyword in normalized for keyword in ("孔", "hole", "holes", "drill")):
            return FeatureRecognitionResult(
                operation_type="drilling",
                confidence=0.9,
                reason="Feature text matches hole/drilling keywords.",
            )
        if any(keyword in normalized for keyword in ("平面", "plane", "face", "flat")):
            return FeatureRecognitionResult(
                operation_type="finishing",
                confidence=0.85,
                reason="Feature text matches plane/face keywords.",
            )
        if any(keyword in normalized for keyword in ("型腔", "腔", "pocket", "cavity", "feature_001")):
            return FeatureRecognitionResult(
                operation_type="pocket",
                confidence=0.85,
                reason="Feature text matches pocket/cavity keywords.",
            )
        return FeatureRecognitionResult(
            operation_type="",
            confidence=0.0,
            reason="No matching feature keyword.",
        )
