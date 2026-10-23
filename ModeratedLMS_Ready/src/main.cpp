#include "Database.h"
#include "LmsService.h"
#include <exception>
#include <iostream>

int main() {
    try {
        Database database("lms.db");
        database.initialiseSchema();
        LmsService service(database);
        std::cout << "Moderated Learning Management System (demo)\n";
        service.runDemonstration();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Application error: " << ex.what() << '\n';
        return 1;
    }
}
