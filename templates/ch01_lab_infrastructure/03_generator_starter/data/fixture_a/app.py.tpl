# fixture_a — three-checkpoint template (pure Python, '#'-prefixed blocks)
"""Fixture template A for the ch01 generator-starter exercise.

Three sequential checkpoints over one small data-processing module.
"""


def total(samples):
%LABS-BEGIN 1
%LABS-SOLUTION
    return sum(samples)
%LABS-STUB
    # TODO(1): return the sum of samples
    return 0
%LABS-END


def scaled(samples, factor):
%LABS-BEGIN 2
%LABS-SOLUTION
    return [value * factor for value in samples]
%LABS-STUB
    # TODO(2): return samples scaled element-wise by factor
    return []
%LABS-END


def summary(samples):
%LABS-BEGIN 3
%LABS-SOLUTION
    return {
        "count": len(samples),
        "min": min(samples),
        "max": max(samples),
    }
%LABS-STUB
    # TODO(3): return a dict with count/min/max of samples
    return {}
%LABS-END


if __name__ == "__main__":
    print(summary(scaled([1, 2, 3], 10)))
