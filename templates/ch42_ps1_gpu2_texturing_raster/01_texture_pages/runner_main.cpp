#include "../shared/runner_cli.hpp"
#include "tex_stages.hpp"

int main(int argc, char** argv) {
    return psx::gpu::labs_runner_main<
        psx::gpu::GpuDevice<psx::gpu::TexPagesStages>>(argc, argv);
}
