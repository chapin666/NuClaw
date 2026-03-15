#include <iostream>
#include "nuclaw/tenant.hpp"

int main() {
    std::cout << "Test tenant.hpp...\n";
    nuclaw::TenantContext ctx;
    ctx.tenant_id = "test";
    std::cout << "Tenant: " << ctx.tenant_id << "\n";
    std::cout << "Test completed!\n";
    return 0;
}
