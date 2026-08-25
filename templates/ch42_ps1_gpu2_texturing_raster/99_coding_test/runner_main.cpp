#include "../shared/runner_cli.hpp"
#include "coding_stages.hpp"

int main(int argc, char** argv) {
    return psx::gpu::labs_runner_main<
        psx::gpu::GpuDevice<psx::gpu::CodingTestStages>>(argc, argv);
}
