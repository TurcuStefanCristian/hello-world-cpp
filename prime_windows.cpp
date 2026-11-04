
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

using namespace std;

const int NUM_PROCESSES = 10;
const int LIMIT = 10000;

bool isPrime(int n) {
    if (n < 2) return false;

    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }

    return true;
}

bool writeAll(HANDLE pipe, const void* data, DWORD size) {
    const char* buffer = static_cast<const char*>(data);
    DWORD total = 0;

    while (total < size) {
        DWORD written = 0;

        if (!WriteFile(pipe, buffer + total, size - total,
                       &written, NULL) || written == 0) {
            return false;
        }

        total += written;
    }

    return true;
}

bool readAll(HANDLE pipe, void* data, DWORD size) {
    char* buffer = static_cast<char*>(data);
    DWORD total = 0;

    while (total < size) {
        DWORD received = 0;

        if (!ReadFile(pipe, buffer + total, size - total,
                      &received, NULL) || received == 0) {
            return false;
        }

        total += received;
    }

    return true;
}

int runChild() {
    int start, end;

    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!readAll(input, &start, sizeof(start)) ||
        !readAll(input, &end, sizeof(end))) {
        return 1;
    }

    for (int n = start; n <= end; n++) {
        if (isPrime(n)) {
            if (!writeAll(output, &n, sizeof(n))) {
                return 1;
            }
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && string(argv[1]) == "--child") {
        return runChild();
    }

    char executable[MAX_PATH];

    if (GetModuleFileNameA(NULL, executable, MAX_PATH) == 0) {
        cerr << "Eroare la obtinerea caii programului.\n";
        return 1;
    }

    struct Worker {
        HANDLE inputWrite;
        HANDLE outputRead;
        PROCESS_INFORMATION process;
        int start;
        int end;
    };

    vector<Worker> workers(NUM_PROCESSES);

    SECURITY_ATTRIBUTES security;
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.lpSecurityDescriptor = NULL;
    security.bInheritHandle = TRUE;

    // Cream si pornim cele 10 procese copil.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        HANDLE childInputRead, parentInputWrite;
        HANDLE parentOutputRead, childOutputWrite;

        if (!CreatePipe(&childInputRead, &parentInputWrite,
                        &security, 0) ||
            !CreatePipe(&parentOutputRead, &childOutputWrite,
                        &security, 0)) {
            cerr << "Eroare la crearea pipe-urilor.\n";
            return 1;
        }

        // Capetele folosite de parinte nu trebuie mostenite.
        SetHandleInformation(parentInputWrite,
                             HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(parentOutputRead,
                             HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA startup;
        ZeroMemory(&startup, sizeof(startup));
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = childInputRead;
        startup.hStdOutput = childOutputWrite;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION process;
        ZeroMemory(&process, sizeof(process));

        string command = "\"" + string(executable) + "\" --child";
        vector<char> commandLine(command.begin(), command.end());
        commandLine.push_back('\0');

        BOOL created = CreateProcessA(
            executable,
            commandLine.data(),
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &startup,
            &process
        );

        CloseHandle(childInputRead);
        CloseHandle(childOutputWrite);

        if (!created) {
            cerr << "Eroare la crearea procesului " << i + 1 << ".\n";
            CloseHandle(parentInputWrite);
            CloseHandle(parentOutputRead);
            return 1;
        }

        int start = i * 1000 + 1;
        int end = (i + 1) * 1000;

        workers[i] = {
            parentInputWrite,
            parentOutputRead,
            process,
            start,
            end
        };
    }

    cout << "Au fost create 10 procese copil.\n";
    cout << "Cautarea numerelor prime pana la 10000:\n\n";

    // Parintele trimite intervalul fiecarui proces prin pipe.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        writeAll(workers[i].inputWrite,
                 &workers[i].start, sizeof(int));

        writeAll(workers[i].inputWrite,
                 &workers[i].end, sizeof(int));

        CloseHandle(workers[i].inputWrite);
    }

    // Parintele primeste si afiseaza numerele prime.
    int totalPrimes = 0;

    for (int i = 0; i < NUM_PROCESSES; i++) {
        int prime;

        while (readAll(workers[i].outputRead,
                       &prime, sizeof(int))) {
            cout << prime << " ";
            totalPrimes++;
        }

        CloseHandle(workers[i].outputRead);

        WaitForSingleObject(workers[i].process.hProcess, INFINITE);

        CloseHandle(workers[i].process.hProcess);
        CloseHandle(workers[i].process.hThread);
    }

    cout << "\n\nTotal numere prime: " << totalPrimes << "\n";

    return 0;
}
