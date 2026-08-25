#include "../shared/runner_cli.hpp"
#include "challenge_gpu.hpp"

int main(int argc, char** argv) {
    return psx::gpu::labs_runner_main<
        psx::gpu::GpuDevice<psx::gpu::ChallengeStages>>(argc, argv);
}
