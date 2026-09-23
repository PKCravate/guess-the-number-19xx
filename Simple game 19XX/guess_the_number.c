/*
 * Guess the Number - V1 (mode nombres)
 * Le nombre à deviner n'est pas aléatoire : on te donne un nombre de départ,
 * puis une suite d'opérations défile très vite. Si tu suis tout, tu trouves
 * du premier coup. Sinon, c'est un guess the number classique (plus / moins).
 *
 * Linux   : gcc -std=c99 -Wall -o guess guess_the_number.c
 * Windows : gcc -std=c99 -Wall -o guess.exe guess_the_number.c   (MinGW)
 */

#ifndef _WIN32
  #define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
#endif

#define MIN_VALUE 1
#define MAX_VALUE 100

typedef struct {
    const char *name;
    int ops;       /* nombre d'opérations affichées */
    int max_step;  /* valeur max de chaque opération (<= 50) */
    int ms;        /* durée d'affichage de chaque opération */
} Difficulty;

static const Difficulty DIFFICULTIES[] = {
    { "Facile",    5,  9, 1200 },
    { "Moyen",     8, 20,  800 },
    { "Difficile", 12, 50, 500 },
};

/* ---------- utilitaires plateforme ---------- */

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static void init_console(void)
{
#ifdef _WIN32
    /* UTF-8 pour les accents + activation des codes ANSI */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
    }
#endif
}

static void clear_screen(void)
{
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* Lit un entier. Retourne 1 si ok, 0 si fin d'entrée (Ctrl+D / Ctrl+Z). */
static int read_int(const char *prompt, int *out)
{
    char buf[64];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) return 0;
        char *end;
        long v = strtol(buf, &end, 10);
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
        if (end != buf && *end == '\0') {
            *out = (int)v;
            return 1;
        }
        printf("Tape juste un nombre.\n");
    }
}

static int wait_enter(void)
{
    char buf[64];
    fflush(stdout);
    return fgets(buf, sizeof buf, stdin) != NULL;
}

/* ---------- jeu ---------- */

/* Retourne le nombre d'essais, ou -1 si l'entrée est fermée. */
static int play_round(const Difficulty *d)
{
    int value = 10 + rand() % 41; /* départ entre 10 et 50 */

    clear_screen();
    printf("=== Mode nombres - %s ===\n\n", d->name);
    printf("Nombre de départ : %d\n\n", value);
    printf("%d opérations vont défiler. Retiens bien.\n", d->ops);
    printf("Appuie sur Entrée quand tu es prêt...");
    if (!wait_enter()) return -1;

    for (int i = 3; i >= 1; i--) {
        clear_screen();
        printf("\n\n\n          %d...\n", i);
        fflush(stdout);
        sleep_ms(700);
    }

    for (int i = 0; i < d->ops; i++) {
        int step = 1 + rand() % d->max_step;
        int add = rand() % 2;

        /* on garde le résultat entre MIN_VALUE et MAX_VALUE */
        if (!add && value - step < MIN_VALUE) add = 1;
        if (add && value + step > MAX_VALUE) add = 0;

        value += add ? step : -step;

        clear_screen();
        printf("\n\n\n          %c %d\n", add ? '+' : '-', step);
        fflush(stdout);
        sleep_ms(d->ms);

        /* écran vide très court : sinon deux "+ 5" d'affilée se confondent */
        clear_screen();
        sleep_ms(150);
    }

    clear_screen();
    printf("Le nombre est entre %d et %d.\n\n", MIN_VALUE, MAX_VALUE);

    int tries = 0, guess;
    for (;;) {
        if (!read_int("Ta proposition : ", &guess)) return -1;
        tries++;
        if (guess < value)      printf("  C'est plus.\n");
        else if (guess > value) printf("  C'est moins.\n");
        else break;
    }

    printf("\nTrouvé ! C'était %d, en %d essai%s.\n", value, tries, tries > 1 ? "s" : "");
    if (tries == 1)      printf("Calcul parfait, tu as tout suivi.\n");
    else if (tries <= 3) printf("Presque tout suivi, pas mal.\n");
    else                 printf("Mode devinette classique. Ça compte aussi.\n");

    return tries;
}

int main(void)
{
    init_console();
    srand((unsigned)time(NULL));

    int best[3] = { 0, 0, 0 };
    const int n_diff = (int)(sizeof DIFFICULTIES / sizeof DIFFICULTIES[0]);

    for (;;) {
        clear_screen();
        printf("=== GUESS THE NUMBER ===\n\n");
        for (int i = 0; i < n_diff; i++) {
            printf("  %d. %s", i + 1, DIFFICULTIES[i].name);
            if (best[i] > 0) printf("   (record : %d essai%s)", best[i], best[i] > 1 ? "s" : "");
            printf("\n");
        }
        printf("  0. Quitter\n\n");

        int choice;
        if (!read_int("Choix : ", &choice) || choice == 0) break;
        if (choice < 1 || choice > n_diff) continue;

        int tries = play_round(&DIFFICULTIES[choice - 1]);
        if (tries < 0) break;
        if (best[choice - 1] == 0 || tries < best[choice - 1]) best[choice - 1] = tries;

        printf("\nEntrée pour revenir au menu...");
        if (!wait_enter()) break;
    }

    printf("\nÀ plus !\n");
    return 0;
}
