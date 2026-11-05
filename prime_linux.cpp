#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <cerrno>

using namespace std;

const int NUM_PROCESSES = 10;
const int LIMIT = 10000;

bool isPrime(int n) {
    if (n < 2) return false;

    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0)
            return false;
    }

    return true;
}

int main() {
    int inputPipes[NUM_PROCESSES][2];
    int outputPipes[NUM_PROCESSES][2];
    pid_t processes[NUM_PROCESSES];

    // Cream cele 10 perechi de pipe-uri.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (pipe(inputPipes[i]) == -1 ||
            pipe(outputPipes[i]) == -1) {
            perror("Eroare la crearea pipe-ului");
            return 1;
        }
    }

    // Cream cele 10 procese copil.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        processes[i] = fork();

        if (processes[i] == -1) {
            perror("Eroare la fork");
            return 1;
        }

        if (processes[i] == 0) {
            // Proces copil: inchide capetele nefolosite.
            close(inputPipes[i][1]);
            close(outputPipes[i][0]);

            for (int j = 0; j < NUM_PROCESSES; j++) {
                if (j != i) {
                    close(inputPipes[j][0]);
                    close(inputPipes[j][1]);
                    close(outputPipes[j][0]);
                    close(outputPipes[j][1]);
                }
            }

            int start, end;

            if (read(inputPipes[i][0], &start, sizeof(start))
                    != sizeof(start) ||
                read(inputPipes[i][0], &end, sizeof(end))
                    != sizeof(end)) {
                close(inputPipes[i][0]);
                close(outputPipes[i][1]);
                _exit(1);
            }

            close(inputPipes[i][0]);

            // Cautam numerele prime din intervalul primit.
            for (int n = start; n <= end; n++) {
                if (isPrime(n)) {
                    write(outputPipes[i][1], &n, sizeof(n));
                }
            }

            // Trimitem valoarea 0 ca marcator de final.
            int finish = 0;
            write(outputPipes[i][1], &finish, sizeof(finish));

            close(outputPipes[i][1]);
            _exit(0);
        }
    }

    // Procesul parinte inchide capetele nefolosite.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        close(inputPipes[i][0]);
        close(outputPipes[i][1]);
    }

    // Trimitem cate un interval de 1000 de numere fiecarui copil.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int start = i * 1000 + 1;
        int end = (i + 1) * 1000;

        write(inputPipes[i][1], &start, sizeof(start));
        write(inputPipes[i][1], &end, sizeof(end));

        close(inputPipes[i][1]);
    }

    cout << "Numere prime gasite pana la 10000:\n";

    int total = 0;

    // Citim rezultatele trimise de fiecare proces copil.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int number;

        while (read(outputPipes[i][0], &number, sizeof(number))
                == sizeof(number)) {
            if (number == 0)
                break;

            cout << number << " ";
            total++;
        }

        close(outputPipes[i][0]);
    }

    // Asteptam terminarea tuturor proceselor copil.
    for (int i = 0; i < NUM_PROCESSES; i++) {
        waitpid(processes[i], nullptr, 0);
    }

    cout << "\n\nTotal numere prime: " << total << endl;

    return 0;
}