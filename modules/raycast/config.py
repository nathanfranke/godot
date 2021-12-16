def can_build(env, platform):
    # Depends on Embree library, which only supports x86_64 and arm64.

    if platform == "javascript":
        return False  # No SIMD support yet
    elif platform == "osx":
        return True  # Universal, x86_64, and arm64 will all work.

    return env["arch"] in ["x86_64", "arm64"]


def configure(env):
    pass
