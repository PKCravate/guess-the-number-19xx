/*
 * Guess the Number 19XX - version graphique (SDL2)
 *
 * La réponse n'est pas aléatoire : on part d'un état affiché, puis une suite
 * d'opérations défile très vite. Si tu suis tout, tu trouves du premier coup.
 * Sinon, c'est un guess the number classique avec des indices plus / moins.
 *
 * Modes : Nombres (calcul), Horloge (modulo 24), Dés à symboles (mémoire).
 * Options : mode série (un seul essai par manche, jusqu'à la première erreur),
 * records sauvegardés dans records.txt à côté de l'exécutable.
 *
 * Windows (MSYS2 UCRT64) :
 *   gcc -std=c99 -Wall -O2 -o guess_gui.exe guess_gui.c $(pkg-config --cflags --static --libs sdl2) -static
 * Linux :
 *   gcc -std=c99 -Wall -O2 -o guess_gui guess_gui.c $(sdl2-config --cflags --libs)
 *
 * Aucune autre dépendance : la police bitmap et les symboles sont dans ce fichier.
 */

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 960
#define H 600

/* ---------- réglages ---------- */

enum { MODE_NUM, MODE_CLOCK, MODE_DICE, N_MODES };

typedef struct {
    const char *name;
    int ops;        /* nombre d'opérations affichées */
    int max_step;   /* valeur max de chaque opération (nombres, heures) */
    int ms;         /* durée d'affichage de chaque opération */
    int faces;      /* mode dés : nombre de faces */
    int start_dice; /* mode dés : dés au départ */
} Difficulty;

static const Difficulty DIFFICULTIES[] = {
    { "Facile",     5,  9, 2000,  6, 2 },
    { "Moyen",      8, 20, 2000,  8, 3 },
    { "Difficile", 12, 50, 2000, 12, 4 },
};
#define N_DIFF 3

#define GAP_MS   150 /* écran vide entre deux opérations */
#define COUNT_MS 700 /* durée de chaque chiffre du compte à rebours */
#define MAX_OPS  16
#define N_SYMS   8
#define MAX_HIST 32

/* ---------- langues ---------- */

enum { LANG_FR, LANG_EN, LANG_ES, LANG_DE, N_LANGS };

static const char *LANG_NAMES[N_LANGS] = { "Français", "English", "Español", "Deutsch" };
static const char *LANG_CODES[N_LANGS] = { "fr", "en", "es", "de" };

/* Chaque texte existe dans les 4 langues, dans l'ordre FR, EN, ES, DE.
 * Les groupes (modes, difficultés, symboles...) sont consécutifs : S_MODE0 + i. */
typedef enum {
    S_SUBTITLE, S_MODE_LABEL, S_DIFF_LABEL,
    S_MODE0, S_MODE1, S_MODE2,
    S_MODE_UP0, S_MODE_UP1, S_MODE_UP2,
    S_SKILL0, S_SKILL1, S_SKILL2,
    S_DIFF0, S_DIFF1, S_DIFF2,
    S_DIFF_UP0, S_DIFF_UP1, S_DIFF_UP2,
    S_TRY1, S_TRYN, S_RECORD, S_RECORD_NONE, S_BEST_STREAK,
    S_SERIE_MODE, S_NOTE_SERIE, S_NOTE_NORMAL, S_PLAY, S_OPTIONS_BTN, S_QUIT, S_MENU_HINT,
    S_OPTIONS_TITLE,
    S_OPT0, S_OPT1, S_OPT2, S_OPT3, S_OPT4, S_OPT5, S_OPT6,
    S_HELP0, S_HELP1, S_HELP2, S_HELP3, S_HELP4, S_HELP5, S_HELP6,
    S_FULLSCREEN, S_AUTO, S_YES, S_NO, S_ON, S_OFF, S_DEFAULTS, S_BACK, S_OPT_HINT,
    S_START_NUM, S_START_CLOCK, S_CLOCK_WARMUP, S_START_DICE, S_REMEMBER,
    S_OPS_COMING, S_PER_OP, S_RANGE_NUM, S_GIVE_CLOCK, S_GIVE_DICE, S_CLICK_START, S_ESC_MENU,
    S_READY, S_JUMP_FUTURE, S_JUMP_PAST, S_DIE_ADD, S_DIE_DEL,
    S_FUT0, S_FUT1, S_FUT2, S_FUT3, S_FUT4, S_FUT5, S_FUT6, S_FUT7,
    S_PAST0, S_PAST1, S_PAST2, S_PAST3, S_PAST4, S_PAST5, S_PAST6, S_PAST7,
    S_SYM0, S_SYM1, S_SYM2, S_SYM3, S_SYM4, S_SYM5, S_SYM6, S_SYM7,
    S_Q_NUM, S_Q_CLOCK, S_Q_DICE, S_BETWEEN, S_ANSWER_RANGE,
    S_TRIES_LABEL, S_ONE_TRY, S_STREAK_FMT, S_HISTORY,
    S_BIG_MORE, S_BIG_LESS, S_BIG_LATER, S_BIG_EARLIER,
    S_MORE, S_LESS, S_LATER, S_EARLIER,
    S_GUESS_HINT, S_GUESS_HINT_NEG,
    S_PERFECT, S_FOUND, S_MISSED, S_WAS_ANSWERED, S_WAS_INDEED, S_WAS_TRIES, S_DICE_LEFT,
    S_STREAK_GOES, S_FIRST_ROUND, S_STREAK_OVER, S_COMMENT1, S_COMMENT3, S_COMMENTN,
    S_ROUNDS_WON, S_NEW_RECORD, S_REPLAY, S_NEXT_ROUND, S_NEW_STREAK, S_MENU, S_RESULT_HINT,
    S_HEADER_STREAK, S_CLOCK_FMT, S_CLOCK_SHORT, S_CLOCK_SUFFIX,
    N_STR
} StrId;

static const char *STR[N_STR][N_LANGS] = {
    [S_SUBTITLE]     = { "- 19XX -  suis le calcul... ou devine.", "- 19XX -  follow the math... or guess.",
                         "- 19XX -  sigue el cálculo... o adivina.", "- 19XX -  rechne mit... oder rate." },
    [S_MODE_LABEL]   = { "MODE", "MODE", "MODO", "MODUS" },
    [S_DIFF_LABEL]   = { "DIFFICULTÉ", "DIFFICULTY", "DIFICULTAD", "SCHWIERIGKEIT" },

    [S_MODE0]        = { "Nombres", "Numbers", "Números", "Zahlen" },
    [S_MODE1]        = { "Horloge", "Clock", "Reloj", "Uhr" },
    [S_MODE2]        = { "Dés", "Dice", "Dados", "Würfel" },
    [S_MODE_UP0]     = { "NOMBRES", "NUMBERS", "NÚMEROS", "ZAHLEN" },
    [S_MODE_UP1]     = { "HORLOGE", "CLOCK", "RELOJ", "UHR" },
    [S_MODE_UP2]     = { "DÉS", "DICE", "DADOS", "WÜRFEL" },
    [S_SKILL0]       = { "Calcul mental", "Mental math", "Cálculo mental", "Kopfrechnen" },
    [S_SKILL1]       = { "Calcul modulo 24", "Modulo 24 math", "Cálculo módulo 24", "Rechnen modulo 24" },
    [S_SKILL2]       = { "Mémoire", "Memory", "Memoria", "Gedächtnis" },
    [S_DIFF0]        = { "Facile", "Easy", "Fácil", "Leicht" },
    [S_DIFF1]        = { "Moyen", "Medium", "Medio", "Mittel" },
    [S_DIFF2]        = { "Difficile", "Hard", "Difícil", "Schwer" },
    [S_DIFF_UP0]     = { "FACILE", "EASY", "FÁCIL", "LEICHT" },
    [S_DIFF_UP1]     = { "MOYEN", "MEDIUM", "MEDIO", "MITTEL" },
    [S_DIFF_UP2]     = { "DIFFICILE", "HARD", "DIFÍCIL", "SCHWER" },

    [S_TRY1]         = { "essai", "try", "intento", "Versuch" },
    [S_TRYN]         = { "essais", "tries", "intentos", "Versuche" },
    [S_RECORD]       = { "Record : %d %s", "Best: %d %s", "Récord: %d %s", "Rekord: %d %s" },
    [S_RECORD_NONE]  = { "Record : -", "Best: -", "Récord: -", "Rekord: -" },
    [S_BEST_STREAK]  = { "Meilleure série : %d", "Best streak: %d", "Mejor racha: %d", "Beste Serie: %d" },
    [S_SERIE_MODE]   = { "Mode série", "Streak mode", "Modo racha", "Serienmodus" },
    [S_NOTE_SERIE]   = { "Un seul essai par manche, on enchaîne jusqu'à la première erreur.",
                         "One guess per round, keep going until the first mistake.",
                         "Un solo intento por ronda, sigue hasta el primer error.",
                         "Ein Versuch pro Runde, weiter bis zum ersten Fehler." },
    [S_NOTE_NORMAL]  = { "Score = nombre d'essais. 1 essai : tu as tout suivi.",
                         "Score = number of tries. 1 try: you followed everything.",
                         "Puntuación = intentos. 1 intento: lo seguiste todo.",
                         "Punkte = Anzahl der Versuche. 1 Versuch: alles verfolgt." },
    [S_PLAY]         = { "JOUER", "PLAY", "JUGAR", "SPIELEN" },
    [S_OPTIONS_BTN]  = { "Options", "Options", "Opciones", "Optionen" },
    [S_QUIT]         = { "Quitter", "Quit", "Salir", "Beenden" },
    [S_MENU_HINT]    = { "Flèches : choisir  S : série  O : options  Entrée : jouer  Échap : quitter",
                         "Arrows: choose  S: streak  O: options  Enter: play  Esc: quit",
                         "Flechas: elegir  S: racha  O: opciones  Intro: jugar  Esc: salir",
                         "Pfeile: wählen  S: Serie  O: Optionen  Enter: spielen  Esc: beenden" },

    [S_OPTIONS_TITLE] = { "OPTIONS", "OPTIONS", "OPCIONES", "OPTIONEN" },
    [S_OPT0]         = { "Langue", "Language", "Idioma", "Sprache" },
    [S_OPT1]         = { "Taille de l'écran", "Screen size", "Tamaño de pantalla", "Bildschirmgröße" },
    [S_OPT2]         = { "Vitesse de défilement", "Scroll speed", "Velocidad", "Geschwindigkeit" },
    [S_OPT3]         = { "Valeur des opérations", "Operation size", "Valor de operaciones", "Operationswert" },
    [S_OPT4]         = { "Nombre maximum", "Maximum number", "Número máximo", "Höchstzahl" },
    [S_OPT5]         = { "Résultats négatifs", "Negative results", "Resultados negativos", "Negative Ergebnisse" },
    [S_OPT6]         = { "Musique", "Music", "Música", "Musik" },
    [S_HELP0]        = { "Langue des textes du jeu.", "Language of the game's texts.",
                         "Idioma de los textos del juego.", "Sprache der Spieltexte." },
    [S_HELP1]        = { "Taille de la fenêtre. Elle se redimensionne aussi à la souris.",
                         "Window size. You can also resize it with the mouse.",
                         "Tamaño de la ventana. También se cambia con el ratón.",
                         "Fenstergröße. Sie lässt sich auch mit der Maus ändern." },
    [S_HELP2]        = { "Durée d'affichage de chaque opération. Auto : selon la difficulté.",
                         "How long each operation is shown. Auto: based on difficulty.",
                         "Tiempo que se muestra cada operación. Auto: según la dificultad.",
                         "Anzeigedauer jeder Operation. Auto: je nach Schwierigkeit." },
    [S_HELP3]        = { "Plus grande opération (nombres et heures). Auto : selon la difficulté.",
                         "Largest operation (numbers and hours). Auto: based on difficulty.",
                         "Operación más grande (números y horas). Auto: según la dificultad.",
                         "Größte Operation (Zahlen und Stunden). Auto: je nach Schwierigkeit." },
    [S_HELP4]        = { "Mode Nombres : le résultat ne dépasse jamais ce nombre.",
                         "Numbers mode: the result never goes above this number.",
                         "Modo Números: el resultado nunca supera este número.",
                         "Zahlenmodus: das Ergebnis übersteigt nie diese Zahl." },
    [S_HELP5]        = { "Mode Nombres : le résultat peut descendre jusqu'à -maximum.",
                         "Numbers mode: the result can go down to -maximum.",
                         "Modo Números: el resultado puede bajar hasta -máximo.",
                         "Zahlenmodus: das Ergebnis kann bis -Maximum sinken." },
    [S_HELP6]        = { "La musique arrive bientôt : le réglage est déjà sauvegardé.",
                         "Music is coming soon: the setting is already saved.",
                         "La música llegará pronto: el ajuste ya se guarda.",
                         "Musik kommt bald: die Einstellung wird schon gespeichert." },
    [S_FULLSCREEN]   = { "Plein écran", "Fullscreen", "Completa", "Vollbild" },
    [S_AUTO]         = { "Auto", "Auto", "Auto", "Auto" },
    [S_YES]          = { "Oui", "Yes", "Sí", "Ja" },
    [S_NO]           = { "Non", "No", "No", "Nein" },
    [S_ON]           = { "Activée", "On", "Activada", "An" },
    [S_OFF]          = { "Désactivée", "Off", "Desactivada", "Aus" },
    [S_DEFAULTS]     = { "Par défaut", "Defaults", "Por defecto", "Standard" },
    [S_BACK]         = { "Retour", "Back", "Volver", "Zurück" },
    [S_OPT_HINT]     = { "Haut/Bas : choisir   Gauche/Droite : changer   Échap : retour",
                         "Up/Down: choose   Left/Right: change   Esc: back",
                         "Arriba/Abajo: elegir   Izq./Der.: cambiar   Esc: volver",
                         "Hoch/Runter: wählen   Links/Rechts: ändern   Esc: zurück" },

    [S_START_NUM]    = { "Nombre de départ", "Starting number", "Número inicial", "Startzahl" },
    [S_START_CLOCK]  = { "Heure de départ", "Starting time", "Hora de salida", "Startzeit" },
    [S_CLOCK_WARMUP] = { "La machine à voyager dans le temps chauffe...", "The time machine is warming up...",
                         "La máquina del tiempo se está calentando...", "Die Zeitmaschine wärmt sich auf..." },
    [S_START_DICE]   = { "Dés de départ (d%d)", "Starting dice (d%d)", "Dados iniciales (d%d)", "Startwürfel (W%d)" },
    [S_REMEMBER]     = { "Retiens bien quel symbole vaut quoi !", "Remember which symbol is worth what!",
                         "¡Recuerda cuánto vale cada símbolo!", "Merk dir, was jedes Symbol wert ist!" },
    [S_OPS_COMING]   = { "%d opérations vont défiler.", "%d operations will flash by.",
                         "Van a pasar %d operaciones.", "%d Operationen laufen gleich durch." },
    [S_PER_OP]       = { "%s par opération", "%s per operation", "%s por operación", "%s pro Operation" },
    [S_RANGE_NUM]    = { "Le résultat reste entre %d et %d.", "The result stays between %d and %d.",
                         "El resultado queda entre %d y %d.", "Das Ergebnis bleibt zwischen %d und %d." },
    [S_GIVE_CLOCK]   = { "Donne l'heure d'arrivée, entre 0h et 23h.", "Give the arrival time, from 0 to 23.",
                         "Da la hora de llegada, de 0 a 23.", "Gib die Ankunftszeit an, von 0 bis 23." },
    [S_GIVE_DICE]    = { "Donne la somme des dés restants.", "Give the sum of the remaining dice.",
                         "Da la suma de los dados restantes.", "Gib die Summe der übrigen Würfel an." },
    [S_CLICK_START]  = { "Entrée ou clic pour lancer", "Enter or click to start",
                         "Intro o clic para empezar", "Enter oder Klick zum Starten" },
    [S_ESC_MENU]     = { "Échap : menu", "Esc: menu", "Esc: menú", "Esc: Menü" },
    [S_READY]        = { "Prêt ?", "Ready?", "¿Listo?", "Bereit?" },
    [S_JUMP_FUTURE]  = { "(saut dans le futur)", "(jump into the future)", "(salto al futuro)", "(Sprung in die Zukunft)" },
    [S_JUMP_PAST]    = { "(saut dans le passé)", "(jump into the past)", "(salto al pasado)", "(Sprung in die Vergangenheit)" },
    [S_DIE_ADD]      = { "Ajout : dé %s", "Add: %s die", "Añadir: dado %s", "Dazu: %s-Würfel" },
    [S_DIE_DEL]      = { "Retrait : dé %s", "Remove: %s die", "Quitar: dado %s", "Weg: %s-Würfel" },

    [S_FUT0] = { "Bienvenue en l'an 3000.", "Welcome to the year 3000.",
                 "Bienvenido al año 3000.", "Willkommen im Jahr 3000." },
    [S_FUT1] = { "Les robots vous saluent poliment.", "The robots greet you politely.",
                 "Los robots te saludan con educación.", "Die Roboter grüßen dich höflich." },
    [S_FUT2] = { "Les voitures volent, enfin.", "Cars can fly, at last.",
                 "Por fin los coches vuelan.", "Die Autos fliegen, endlich." },
    [S_FUT3] = { "Une colonie sur Mars vous attend.", "A colony on Mars awaits you.",
                 "Una colonia en Marte te espera.", "Eine Marskolonie erwartet dich." },
    [S_FUT4] = { "Tout le monde porte des combinaisons argentées.", "Everyone wears silver jumpsuits.",
                 "Todos llevan monos plateados.", "Alle tragen silberne Overalls." },
    [S_FUT5] = { "Le café est servi par un drone.", "A drone serves you coffee.",
                 "Un dron te sirve el café.", "Der Kaffee kommt per Drohne." },
    [S_FUT6] = { "Vous atterrissez sur une station orbitale.", "You land on an orbital station.",
                 "Aterrizas en una estación orbital.", "Du landest auf einer Raumstation." },
    [S_FUT7] = { "Il pleut des néons.", "It is raining neon.",
                 "Llueven luces de neón.", "Es regnet Neonlicht." },
    [S_PAST0] = { "Vous atterrissez en plein Moyen Âge.", "You land in the middle of the Middle Ages.",
                  "Aterrizas en plena Edad Media.", "Du landest mitten im Mittelalter." },
    [S_PAST1] = { "Un dinosaure vous regarde passer.", "A dinosaur watches you go by.",
                  "Un dinosaurio te mira pasar.", "Ein Dinosaurier sieht dir nach." },
    [S_PAST2] = { "Les pyramides sont en chantier.", "The pyramids are under construction.",
                  "Las pirámides están en obras.", "Die Pyramiden sind noch im Bau." },
    [S_PAST3] = { "Un mammouth broute à côté.", "A mammoth grazes nearby.",
                  "Un mamut pasta a tu lado.", "Ein Mammut grast neben dir." },
    [S_PAST4] = { "Napoléon vous demande l'heure.", "Napoleon asks you for the time.",
                  "Napoleón te pregunta la hora.", "Napoleon fragt dich nach der Uhrzeit." },
    [S_PAST5] = { "Vous tombez en pleine Renaissance.", "You drop into the Renaissance.",
                  "Caes en pleno Renacimiento.", "Du platzt mitten in die Renaissance." },
    [S_PAST6] = { "Des Vikings accostent sur la plage.", "Vikings are landing on the beach.",
                  "Unos vikingos desembarcan en la playa.", "Wikinger landen am Strand." },
    [S_PAST7] = { "Un chevalier vous prend pour un sorcier.", "A knight takes you for a wizard.",
                  "Un caballero te toma por un brujo.", "Ein Ritter hält dich für einen Zauberer." },

    [S_SYM0] = { "étoile", "star", "estrella", "Stern" },
    [S_SYM1] = { "coeur", "heart", "corazón", "Herz" },
    [S_SYM2] = { "losange", "diamond", "rombo", "Raute" },
    [S_SYM3] = { "rond", "circle", "círculo", "Kreis" },
    [S_SYM4] = { "triangle", "triangle", "triángulo", "Dreieck" },
    [S_SYM5] = { "carré", "square", "cuadrado", "Quadrat" },
    [S_SYM6] = { "lune", "moon", "luna", "Mond" },
    [S_SYM7] = { "croix", "cross", "cruz", "Kreuz" },

    [S_Q_NUM]        = { "Quel est le nombre ?", "What is the number?", "¿Cuál es el número?", "Wie lautet die Zahl?" },
    [S_Q_CLOCK]      = { "Quelle heure est-il à l'arrivée ?", "What time is it on arrival?",
                         "¿Qué hora es al llegar?", "Wie spät ist es bei der Ankunft?" },
    [S_Q_DICE]       = { "Somme des dés restants ?", "Sum of the remaining dice?",
                         "¿Suma de los dados restantes?", "Summe der übrigen Würfel?" },
    [S_BETWEEN]      = { "Entre %d et %d", "Between %d and %d", "Entre %d y %d", "Zwischen %d und %d" },
    [S_ANSWER_RANGE] = { "Réponds entre %d et %d.", "Answer between %d and %d.",
                         "Responde entre %d y %d.", "Antworte zwischen %d und %d." },
    [S_TRIES_LABEL]  = { "Essais", "Tries", "Intentos", "Versuche" },
    [S_ONE_TRY]      = { "Un seul essai !", "Only one try!", "¡Un solo intento!", "Nur ein Versuch!" },
    [S_STREAK_FMT]   = { "Série : %d", "Streak: %d", "Racha: %d", "Serie: %d" },
    [S_HISTORY]      = { "Historique", "History", "Historial", "Verlauf" },
    [S_BIG_MORE]     = { "C'est plus !", "Higher!", "¡Más alto!", "Höher!" },
    [S_BIG_LESS]     = { "C'est moins !", "Lower!", "¡Más bajo!", "Niedriger!" },
    [S_BIG_LATER]    = { "Plus tard !", "Later!", "¡Más tarde!", "Später!" },
    [S_BIG_EARLIER]  = { "Plus tôt !", "Earlier!", "¡Más temprano!", "Früher!" },
    [S_MORE]         = { "plus", "higher", "más alto", "höher" },
    [S_LESS]         = { "moins", "lower", "más bajo", "niedriger" },
    [S_LATER]        = { "plus tard", "later", "después", "später" },
    [S_EARLIER]      = { "plus tôt", "earlier", "antes", "früher" },
    [S_GUESS_HINT]   = { "Chiffres : saisir   Entrée : valider   Échap : menu",
                         "Digits: type   Enter: confirm   Esc: menu",
                         "Cifras: escribir   Intro: validar   Esc: menú",
                         "Ziffern: tippen   Enter: bestätigen   Esc: Menü" },
    [S_GUESS_HINT_NEG] = { "Chiffres : saisir   - : signe   Entrée : valider   Échap : menu",
                           "Digits: type   -: sign   Enter: confirm   Esc: menu",
                           "Cifras: escribir   -: signo   Intro: validar   Esc: menú",
                           "Ziffern: tippen   -: Vorzeichen   Enter: bestätigen   Esc: Menü" },

    [S_PERFECT]      = { "PARFAIT !", "PERFECT!", "¡PERFECTO!", "PERFEKT!" },
    [S_FOUND]        = { "TROUVÉ !", "FOUND IT!", "¡ACERTASTE!", "GEFUNDEN!" },
    [S_MISSED]       = { "RATÉ !", "MISSED!", "¡FALLASTE!", "DANEBEN!" },
    [S_WAS_ANSWERED] = { "C'était %s. Tu as répondu %s.", "It was %s. You answered %s.",
                         "Era %s. Respondiste %s.", "Es war %s. Du hast %s gesagt." },
    [S_WAS_INDEED]   = { "C'était bien %s.", "It was indeed %s.", "Era %s, en efecto.", "Es war tatsächlich %s." },
    [S_WAS_TRIES]    = { "C'était %s, en %d %s.", "It was %s, in %d %s.", "Era %s, en %d %s.", "Es war %s (%d %s)." },
    [S_DICE_LEFT]    = { "Dés restants :", "Remaining dice:", "Dados restantes:", "Übrige Würfel:" },
    [S_STREAK_GOES]  = { "La série continue !", "The streak goes on!", "¡La racha sigue!", "Die Serie geht weiter!" },
    [S_FIRST_ROUND]  = { "Première manche réussie !", "First round cleared!",
                         "¡Primera ronda superada!", "Erste Runde geschafft!" },
    [S_STREAK_OVER]  = { "Série terminée.", "Streak over.", "Racha terminada.", "Serie beendet." },
    [S_COMMENT1]     = { "Calcul parfait, tu as tout suivi.", "Perfect math, you followed everything.",
                         "Cálculo perfecto, lo seguiste todo.", "Perfekt gerechnet, alles verfolgt." },
    [S_COMMENT3]     = { "Presque tout suivi, pas mal.", "Followed almost everything, not bad.",
                         "Casi todo seguido, nada mal.", "Fast alles verfolgt, nicht schlecht." },
    [S_COMMENTN]     = { "Mode devinette classique. Ça compte aussi.", "Classic guessing mode. That counts too.",
                         "Modo adivinanza clásico. También cuenta.", "Klassisches Raten. Zählt auch." },
    [S_ROUNDS_WON]   = { "Manches réussies : %d   (record : %d)", "Rounds won: %d   (best: %d)",
                         "Rondas ganadas: %d   (récord: %d)", "Gewonnene Runden: %d   (Rekord: %d)" },
    [S_NEW_RECORD]   = { "NOUVEAU RECORD !", "NEW RECORD!", "¡NUEVO RÉCORD!", "NEUER REKORD!" },
    [S_REPLAY]       = { "Rejouer", "Play again", "Otra vez", "Nochmal" },
    [S_NEXT_ROUND]   = { "Manche suivante", "Next round", "Siguiente ronda", "Nächste Runde" },
    [S_NEW_STREAK]   = { "Nouvelle série", "New streak", "Nueva racha", "Neue Serie" },
    [S_MENU]         = { "Menu", "Menu", "Menú", "Menü" },
    [S_RESULT_HINT]  = { "Entrée : %s   Échap : menu", "Enter: %s   Esc: menu",
                         "Intro: %s   Esc: menú", "Enter: %s   Esc: Menü" },
    [S_HEADER_STREAK] = { "SÉRIE", "STREAK", "RACHA", "SERIE" },
    [S_CLOCK_FMT]    = { "%02dh00", "%02d:00", "%02d:00", "%02d:00" },
    [S_CLOCK_SHORT]  = { "%02dh", "%02d:00", "%02d:00", "%02d:00" },
    [S_CLOCK_SUFFIX] = { "h", ":00", ":00", ":00" },
};

#define N_FLAVOR 8

/* ---------- couleurs ---------- */

static const SDL_Color BG     = {  12,  14,  32, 255 };
static const SDL_Color GRID   = {  22,  26,  56, 255 };
static const SDL_Color PANEL  = {  26,  30,  64, 255 };
static const SDL_Color HOVER  = {  40,  46,  96, 255 };
static const SDL_Color CYAN   = {  80, 220, 255, 255 };
static const SDL_Color AMBER  = { 255, 190,  60, 255 };
static const SDL_Color PINK   = { 255,  90, 150, 255 };
static const SDL_Color GREEN  = { 110, 240, 140, 255 };
static const SDL_Color RED    = { 255,  80,  80, 255 };
static const SDL_Color WHITE  = { 235, 235, 245, 255 };
static const SDL_Color DIM    = { 120, 125, 170, 255 };
static const SDL_Color SHADOW = {  60,  20,  60, 255 };
static const SDL_Color DIE_BG = { 244, 240, 228, 255 };
static const SDL_Color DIE_FG = {  30,  30,  40, 255 };

/* ---------- police bitmap 5x7 (+2 lignes de jambage) ---------- */

typedef struct { unsigned char c; const char *r[9]; } Glyph;

static const Glyph FONT[] = {
    { 'A', { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
    { 'B', { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." } },
    { 'C', { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." } },
    { 'D', { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." } },
    { 'E', { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } },
    { 'F', { "#####", "#....", "#....", "####.", "#....", "#....", "#...." } },
    { 'G', { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####" } },
    { 'H', { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
    { 'I', { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." } },
    { 'J', { "..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.." } },
    { 'K', { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" } },
    { 'L', { "#....", "#....", "#....", "#....", "#....", "#....", "#####" } },
    { 'M', { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" } },
    { 'N', { "#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#" } },
    { 'O', { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
    { 'P', { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." } },
    { 'Q', { ".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#" } },
    { 'R', { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" } },
    { 'S', { ".####", "#....", "#....", ".###.", "....#", "....#", "####." } },
    { 'T', { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." } },
    { 'U', { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
    { 'V', { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." } },
    { 'W', { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#." } },
    { 'X', { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" } },
    { 'Y', { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." } },
    { 'Z', { "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" } },

    { 'a', { ".....", ".....", ".###.", "....#", ".####", "#...#", ".####" } },
    { 'b', { "#....", "#....", "####.", "#...#", "#...#", "#...#", "####." } },
    { 'c', { ".....", ".....", ".###.", "#....", "#....", "#...#", ".###." } },
    { 'd', { "....#", "....#", ".####", "#...#", "#...#", "#...#", ".####" } },
    { 'e', { ".....", ".....", ".###.", "#...#", "#####", "#....", ".###." } },
    { 'f', { "..##.", ".#...", "###..", ".#...", ".#...", ".#...", ".#..." } },
    { 'g', { ".....", ".....", ".####", "#...#", "#...#", "#...#", ".####", "....#", ".###." } },
    { 'h', { "#....", "#....", "####.", "#...#", "#...#", "#...#", "#...#" } },
    { 'i', { "..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###." } },
    { 'j', { "...#.", ".....", "..##.", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.." } },
    { 'k', { "#....", "#....", "#..#.", "#.#..", "##...", "#.#..", "#..#." } },
    { 'l', { ".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." } },
    { 'm', { ".....", ".....", "##.#.", "#.#.#", "#.#.#", "#.#.#", "#.#.#" } },
    { 'n', { ".....", ".....", "####.", "#...#", "#...#", "#...#", "#...#" } },
    { 'o', { ".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###." } },
    { 'p', { ".....", ".....", "####.", "#...#", "#...#", "#...#", "####.", "#....", "#...." } },
    { 'q', { ".....", ".....", ".####", "#...#", "#...#", "#...#", ".####", "....#", "....#" } },
    { 'r', { ".....", ".....", "#.##.", "##..#", "#....", "#....", "#...." } },
    { 's', { ".....", ".....", ".####", "#....", ".###.", "....#", "####." } },
    { 't', { ".#...", ".#...", "###..", ".#...", ".#...", ".#..#", "..##." } },
    { 'u', { ".....", ".....", "#...#", "#...#", "#...#", "#...#", ".####" } },
    { 'v', { ".....", ".....", "#...#", "#...#", "#...#", ".#.#.", "..#.." } },
    { 'w', { ".....", ".....", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#." } },
    { 'x', { ".....", ".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#" } },
    { 'y', { ".....", ".....", "#...#", "#...#", "#...#", "#...#", ".####", "....#", ".###." } },
    { 'z', { ".....", ".....", "#####", "...#.", "..#..", ".#...", "#####" } },
    { 1,   { ".....", ".....", ".##..", "..#..", "..#..", "..#..", ".###." } }, /* i sans point */
    { 2,   { ".##..", "#..#.", "#.#..", "#..#.", "#...#", "#...#", "#.##." } }, /* eszett */
    { 3,   { "..#..", ".....", "..#..", "..#..", "..#..", "..#..", "..#.." } }, /* point d'exclamation inversé */
    { 4,   { "..#..", ".....", "..#..", ".#...", "#....", "#...#", ".###." } }, /* point d'interrogation inversé */

    { '0', { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." } },
    { '1', { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." } },
    { '2', { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" } },
    { '3', { "####.", "....#", "....#", ".###.", "....#", "....#", "####." } },
    { '4', { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." } },
    { '5', { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." } },
    { '6', { ".###.", "#....", "#....", "####.", "#...#", "#...#", ".###." } },
    { '7', { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." } },
    { '8', { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." } },
    { '9', { ".###.", "#...#", "#...#", ".####", "....#", "....#", ".###." } },

    { '.',  { ".....", ".....", ".....", ".....", ".....", ".##..", ".##.." } },
    { ',',  { ".....", ".....", ".....", ".....", ".....", ".##..", ".##..", "..#..", ".#..." } },
    { ':',  { ".....", ".##..", ".##..", ".....", ".##..", ".##..", "....." } },
    { '!',  { "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.." } },
    { '?',  { ".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.." } },
    { '\'', { "..#..", "..#..", ".#...", ".....", ".....", ".....", "....." } },
    { '"',  { ".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....." } },
    { '-',  { ".....", ".....", ".....", ".###.", ".....", ".....", "....." } },
    { '+',  { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." } },
    { '=',  { ".....", ".....", "#####", ".....", "#####", ".....", "....." } },
    { '(',  { "...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#." } },
    { ')',  { ".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..." } },
    { '[',  { ".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###." } },
    { ']',  { ".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###." } },
    { '/',  { "....#", "....#", "...#.", "..#..", ".#...", "#....", "#...." } },
    { '<',  { "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#." } },
    { '>',  { ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..." } },
    { '_',  { ".....", ".....", ".....", ".....", ".....", ".....", "#####" } },
    { '*',  { ".....", "..#..", "#.#.#", ".###.", "#.#.#", "..#..", "....." } },
};

enum { ACC_NONE, ACC_ACUTE, ACC_GRAVE, ACC_CIRC, ACC_TREMA, ACC_TILDE, ACC_CEDIL };

static const char *ACCENTS[][2] = {
    { NULL, NULL },
    { "...#.", "..#.." },
    { ".#...", "..#.." },
    { "..#..", ".#.#." },
    { ".#.#.", "....." },
    { ".##.#", "#.##." },
    { "..#..", ".##.." },
};

static const char *const *glyph_tab[128];

static void font_init(void)
{
    for (size_t i = 0; i < sizeof FONT / sizeof FONT[0]; i++)
        glyph_tab[FONT[i].c] = FONT[i].r;
}

/* Décode un caractère UTF-8 et avance le pointeur. */
static unsigned next_cp(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && s[1]) {
        *p += 2;
        return ((s[0] & 0x1Fu) << 6) | (s[1] & 0x3Fu);
    }
    if ((s[0] & 0xF0) == 0xE0 && s[1] && s[2]) {
        *p += 3;
        return ((s[0] & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3Fu);
    }
    *p += 1;
    return '?';
}

/* Lettre accentuée -> lettre de base + accent dessiné par-dessus. */
static unsigned decompose(unsigned cp, int *acc)
{
    *acc = ACC_NONE;
    switch (cp) {
    case 0xE9: *acc = ACC_ACUTE; return 'e';
    case 0xE8: *acc = ACC_GRAVE; return 'e';
    case 0xEA: *acc = ACC_CIRC;  return 'e';
    case 0xEB: *acc = ACC_TREMA; return 'e';
    case 0xE0: *acc = ACC_GRAVE; return 'a';
    case 0xE2: *acc = ACC_CIRC;  return 'a';
    case 0xF9: *acc = ACC_GRAVE; return 'u';
    case 0xFB: *acc = ACC_CIRC;  return 'u';
    case 0xFC: *acc = ACC_TREMA; return 'u';
    case 0xF4: *acc = ACC_CIRC;  return 'o';
    case 0xEE: *acc = ACC_CIRC;  return 1;
    case 0xEF: *acc = ACC_TREMA; return 1;
    case 0xE7: *acc = ACC_CEDIL; return 'c';
    case 0xC9: *acc = ACC_ACUTE; return 'E';
    case 0xC8: *acc = ACC_GRAVE; return 'E';
    case 0xCA: *acc = ACC_CIRC;  return 'E';
    case 0xC0: *acc = ACC_GRAVE; return 'A';
    case 0xC2: *acc = ACC_CIRC;  return 'A';
    case 0xC7: *acc = ACC_CEDIL; return 'C';
    case 0xE1: *acc = ACC_ACUTE; return 'a';
    case 0xED: *acc = ACC_ACUTE; return 1;
    case 0xF3: *acc = ACC_ACUTE; return 'o';
    case 0xFA: *acc = ACC_ACUTE; return 'u';
    case 0xF1: *acc = ACC_TILDE; return 'n';
    case 0xE4: *acc = ACC_TREMA; return 'a';
    case 0xF6: *acc = ACC_TREMA; return 'o';
    case 0xC1: *acc = ACC_ACUTE; return 'A';
    case 0xCD: *acc = ACC_ACUTE; return 'I';
    case 0xD3: *acc = ACC_ACUTE; return 'O';
    case 0xDA: *acc = ACC_ACUTE; return 'U';
    case 0xD1: *acc = ACC_TILDE; return 'N';
    case 0xC4: *acc = ACC_TREMA; return 'A';
    case 0xD6: *acc = ACC_TREMA; return 'O';
    case 0xDC: *acc = ACC_TREMA; return 'U';
    case 0xDF: return 2;
    case 0xA1: return 3;
    case 0xBF: return 4;
    case 0x2212: case 0x2013: case 0x2014: return '-';
    case 0x2019: return '\'';
    case 0xAB: case 0xBB: return '"';
    }
    return cp < 128 ? cp : '?';
}

/* ---------- rendu de base ---------- */

static SDL_Renderer *R;

static void set_col(SDL_Color c) { SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a); }

static void fill(int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect r = { x, y, w, h };
    set_col(c);
    SDL_RenderFillRect(R, &r);
}

static void frame_rect(int x, int y, int w, int h, int t, SDL_Color c)
{
    fill(x, y, w, t, c);
    fill(x, y + h - t, w, t, c);
    fill(x, y, t, h, c);
    fill(x + w - t, y, t, h, c);
}

/* Dessine des lignes de pixels '#' (glyphes, accents, symboles). */
static void draw_rows(const char *const *rows, int n, int x, int y, int s)
{
    for (int r = 0; r < n; r++) {
        if (!rows[r]) continue;
        for (int c = 0; rows[r][c]; c++) {
            if (rows[r][c] != '#') continue;
            SDL_Rect px = { x + c * s, y + r * s, s, s };
            SDL_RenderFillRect(R, &px);
        }
    }
}

static int text_width(const char *s, int sc)
{
    int n = 0;
    while (*s) { next_cp(&s); n++; }
    return n ? n * 6 * sc - sc : 0;
}

static void draw_text(const char *s, int x, int y, int sc, SDL_Color col)
{
    set_col(col);
    while (*s) {
        int acc;
        unsigned b = decompose(next_cp(&s), &acc);
        const char *const *g = glyph_tab[b & 127];
        if (g) draw_rows(g, 9, x, y, sc);
        if (acc >= ACC_ACUTE && acc <= ACC_TILDE) {
            int upper = b >= 'A' && b <= 'Z';
            draw_rows(ACCENTS[acc], 2, x, y + (upper ? -3 * sc : 0), sc);
        } else if (acc == ACC_CEDIL) {
            draw_rows(ACCENTS[acc], 2, x, y + 7 * sc, sc);
        }
        x += 6 * sc;
    }
}

static void vtext(int align, int x, int y, int sc, SDL_Color c, const char *fmt, va_list ap)
{
    char buf[256];
    vsnprintf(buf, sizeof buf, fmt, ap);
    if (align == 1) x -= text_width(buf, sc) / 2;
    else if (align == 2) x -= text_width(buf, sc);
#ifdef TEXT_CHECK
    /* compiler avec -DTEXT_CHECK pour repérer les traductions trop longues */
    if (x < 0 || x + text_width(buf, sc) > W) printf("Texte hors écran : %s\n", buf);
#endif
    draw_text(buf, x, y, sc, c);
}

/* Texte aligné à gauche. */
static void text_l(int x, int y, int sc, SDL_Color c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vtext(0, x, y, sc, c, fmt, ap);
    va_end(ap);
}

/* Texte centré sur cx. */
static void text_c(int cx, int y, int sc, SDL_Color c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vtext(1, cx, y, sc, c, fmt, ap);
    va_end(ap);
}

/* ---------- symboles des dés (9x9) ---------- */

static const char *SYM_SPRITES[N_SYMS][9] = {
    { "....#....", "....#....", "...###...", "#########", ".#######.", "..#####..", "..##.##..", ".##...##.", ".#.....#." },
    { ".##...##.", "####.####", "#########", "#########", ".#######.", "..#####..", "...###...", "....#....", "........." },
    { "....#....", "...###...", "..#####..", ".#######.", "#########", ".#######.", "..#####..", "...###...", "....#...." },
    { "...###...", ".#######.", ".#######.", "#########", "#########", "#########", ".#######.", ".#######.", "...###..." },
    { "....#....", "....#....", "...###...", "...###...", "..#####..", "..#####..", ".#######.", ".#######.", "#########" },
    { ".........", ".#######.", ".#######.", ".#######.", ".#######.", ".#######.", ".#######.", ".#######.", "........." },
    { "..#####..", ".###.....", "###......", "###......", "###......", "###......", "###......", ".###.....", "..#####.." },
    { "...###...", "...###...", "...###...", "#########", "#########", "#########", "...###...", "...###...", "...###..." },
};
static const SDL_Color SYM_COLORS[N_SYMS] = {
    { 230, 160,   0, 255 }, { 220,  40,  60, 255 }, { 200,  50, 200, 255 }, {  40, 110, 230, 255 },
    {  30, 160,  70, 255 }, { 240, 110,  20, 255 }, { 110,  70, 200, 255 }, {   0, 160, 170, 255 },
};

/* Dé avec son symbole en haut et sa valeur en bas (value < 0 : "?"). */
static void draw_die(int x, int y, int size, int sym, int value)
{
    int s = size / 20;
    if (s < 1) s = 1;
    fill(x + 4, y + 6, size, size, (SDL_Color){ 0, 0, 0, 255 });
    fill(x, y, size, size, DIE_BG);
    frame_rect(x, y, size, size, s > 2 ? s / 2 + 1 : 2, DIE_FG);

    set_col(SYM_COLORS[sym]);
    draw_rows(SYM_SPRITES[sym], 9, x + (size - 9 * s) / 2, y + size * 8 / 100, s);

    char buf[12];
    if (value < 0) snprintf(buf, sizeof buf, "?");
    else snprintf(buf, sizeof buf, "%d", value);
    draw_text(buf, x + (size - text_width(buf, s)) / 2, y + size * 55 / 100, s,
              value < 0 ? RED : DIE_FG);
}

typedef struct { int sym, value; } Die;

static void draw_dice_row(const Die *d, int n, int y, int size)
{
    int gap = size / 5;
    int total = n * size + (n - 1) * gap;
    int x = (W - total) / 2;
    for (int i = 0; i < n; i++)
        draw_die(x + i * (size + gap), y, size, d[i].sym, d[i].value);
}

/* ---------- état du jeu ---------- */

typedef enum { OP_ADD, OP_SUB, OP_DIE_ADD, OP_DIE_DEL } OpKind;

typedef struct {
    OpKind kind;
    int value;
    int sym;    /* mode dés */
    int flavor; /* mode horloge */
} Op;

typedef struct {
    int mode, diff;
    int start;                 /* nombres, horloge */
    Die dice[N_SYMS];          /* dés au départ */
    int ndice;
    Die final_dice[N_SYMS];    /* dés restants à la fin */
    int nfinal;
    Op ops[MAX_OPS];
    int nops;
    int answer, lo, hi;
    int tries;
    int hist[MAX_HIST];
    int nhist;
    char input[4];
    int ninput;
    int negative;              /* signe moins tapé dans la saisie */
    int found;
    int ms;                    /* durée d'affichage figée au début de la manche */
} Round;

typedef enum { ST_MENU, ST_OPTIONS, ST_INTRO, ST_COUNT, ST_FLASH, ST_GUESS, ST_RESULT } State;

/* ---------- options (options.txt) ---------- */

enum { OPT_LANG, OPT_SIZE, OPT_SPEED, OPT_STEP, OPT_MAX, OPT_NEG, OPT_MUSIC, N_OPTS };

typedef struct {
    int lang;  /* LANG_FR, LANG_EN, LANG_ES, LANG_DE */
    int size;  /* index dans SIZES, le dernier = plein écran */
    int speed; /* index dans SPEEDS, 0 = selon la difficulté */
    int step;  /* index dans STEPS, 0 = selon la difficulté */
    int maxi;  /* index dans MAXES */
    int neg;   /* résultats négatifs autorisés (mode nombres) */
    int music; /* musique activée : à brancher quand la musique sera ajoutée */
} Options;

static const Options OPT_DEFAULT = { LANG_FR, 0, 0, 0, 1, 0, 1 };
static Options opt = { LANG_FR, 0, 0, 0, 1, 0, 1 };

/* Texte traduit dans la langue choisie. */
#define T(id) STR[id][opt.lang]

static const int SIZES[][2] = { { 960, 600 }, { 1280, 800 }, { 1440, 900 }, { 1920, 1200 } };
#define N_SIZES 5 /* 4 tailles + plein écran */
static const int SPEEDS[] = { 0, 300, 500, 800, 1200, 1600, 2000, 3000, 4000, 5000, 6000, 8000, 10000 };
#define N_SPEEDS 13
static const int STEPS[] = { 0, 5, 9, 20, 50, 99 };
#define N_STEPS 6
static const int MAXES[] = { 50, 100, 200, 500, 999 };
#define N_MAXES 5

static int opt_sel;
static SDL_Window *win;

static State st;
static Uint32 st_t0, now;
static int running = 1;

static int sel_mode, sel_diff, serie;
static int streak;       /* manches réussies dans la série en cours */
static int new_record;
static Round rd;

static int best_tries[N_MODES][N_DIFF];  /* 0 = pas encore de record */
static int best_streak[N_MODES][N_DIFF];

static char msg[96];
static Uint32 msg_t;

static int mx, my, clicked;

static void go(State s)
{
    st = s;
    st_t0 = SDL_GetTicks();
}

static void set_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    msg_t = SDL_GetTicks();
}

/* ---------- records (records.txt) ---------- */

static char rec_path[1024], opt_path[1024];

/* Chemin d'un fichier à côté de l'exécutable. */
static void data_path(char *out, size_t n, const char *file)
{
    char *base = SDL_GetBasePath();
    snprintf(out, n, "%s%s", base ? base : "", file);
    SDL_free(base);
}

/* Lit un petit fichier texte entier dans buf. Retourne 0 si absent. */
static int read_small_file(const char *path, char *buf, size_t size)
{
    SDL_RWops *f = SDL_RWFromFile(path, "rb");
    if (!f) return 0;
    size_t n = SDL_RWread(f, buf, 1, size - 1);
    SDL_RWclose(f);
    buf[n] = '\0';
    return 1;
}

static void records_load(void)
{
    data_path(rec_path, sizeof rec_path, "records.txt");

    char buf[1024];
    if (!read_small_file(rec_path, buf, sizeof buf)) return;

    /* une ligne par mode/difficulté : mode diff meilleur_essais meilleure_serie */
    char *line = buf;
    while (line && *line) {
        int m, d, t, s;
        if (sscanf(line, "%d %d %d %d", &m, &d, &t, &s) == 4 &&
            m >= 0 && m < N_MODES && d >= 0 && d < N_DIFF) {
            best_tries[m][d] = t;
            best_streak[m][d] = s;
        }
        line = strchr(line, '\n');
        if (line) line++;
    }
}

static void records_save(void)
{
    SDL_RWops *f = SDL_RWFromFile(rec_path, "wb");
    if (!f) return;
    char line[64];
    for (int m = 0; m < N_MODES; m++)
        for (int d = 0; d < N_DIFF; d++) {
            int n = snprintf(line, sizeof line, "%d %d %d %d\n", m, d, best_tries[m][d], best_streak[m][d]);
            SDL_RWwrite(f, line, 1, (size_t)n);
        }
    SDL_RWclose(f);
}

static const char *OPT_KEYS[N_OPTS] = { "langue", "taille", "vitesse", "operations", "maximum", "negatifs", "musique" };

static int *opt_field(int i)
{
    switch (i) {
    case OPT_LANG:  return &opt.lang;
    case OPT_SIZE:  return &opt.size;
    case OPT_SPEED: return &opt.speed;
    case OPT_STEP:  return &opt.step;
    case OPT_MAX:   return &opt.maxi;
    case OPT_NEG:   return &opt.neg;
    default:        return &opt.music;
    }
}

static int opt_count(int i)
{
    switch (i) {
    case OPT_LANG:  return N_LANGS;
    case OPT_SIZE:  return N_SIZES;
    case OPT_SPEED: return N_SPEEDS;
    case OPT_STEP:  return N_STEPS;
    case OPT_MAX:   return N_MAXES;
    default:        return 2;
    }
}

/* Langue du système si elle est disponible, sinon le français. */
static int system_lang(void)
{
    int lang = LANG_FR;
    SDL_Locale *loc = SDL_GetPreferredLocales();
    if (!loc) return lang;
    for (int i = 0; loc[i].language; i++) {
        int found = -1;
        for (int l = 0; l < N_LANGS; l++)
            if (strcmp(loc[i].language, LANG_CODES[l]) == 0) found = l;
        if (found >= 0) { lang = found; break; }
    }
    SDL_free(loc);
    return lang;
}

static void options_load(void)
{
    data_path(opt_path, sizeof opt_path, "options.txt");

    char buf[1024];
    if (!read_small_file(opt_path, buf, sizeof buf)) {
        opt.lang = system_lang(); /* premier lancement : langue du système */
        return;
    }

    /* une ligne par option : cle=valeur */
    char *line = buf;
    while (line && *line) {
        char key[32];
        int v;
        if (sscanf(line, "%31[^=]=%d", key, &v) == 2)
            for (int i = 0; i < N_OPTS; i++)
                if (strcmp(key, OPT_KEYS[i]) == 0 && v >= 0 && v < opt_count(i))
                    *opt_field(i) = v;
        line = strchr(line, '\n');
        if (line) line++;
    }
}

static void options_save(void)
{
    SDL_RWops *f = SDL_RWFromFile(opt_path, "wb");
    if (!f) return;
    char line[64];
    for (int i = 0; i < N_OPTS; i++) {
        int n = snprintf(line, sizeof line, "%s=%d\n", OPT_KEYS[i], *opt_field(i));
        SDL_RWwrite(f, line, 1, (size_t)n);
    }
    SDL_RWclose(f);
}

/* Applique la taille de fenêtre choisie, sans dépasser l'écran. */
static void apply_window(void)
{
    if (opt.size >= N_SIZES - 1) {
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
        return;
    }
    SDL_SetWindowFullscreen(win, 0);

    int w = SIZES[opt.size][0], h = SIZES[opt.size][1];
    int disp = SDL_GetWindowDisplayIndex(win);
    SDL_Rect b;
    if (disp >= 0 && SDL_GetDisplayUsableBounds(disp, &b) == 0) {
        int maxh = b.h - 40; /* place pour la barre de titre */
        if (w > b.w || h > maxh) {
            double k = (double)b.w / w;
            if ((double)maxh / h < k) k = (double)maxh / h;
            w = (int)(w * k);
            h = (int)(h * k);
        }
    }
    SDL_SetWindowSize(win, w, h);
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(disp < 0 ? 0 : disp),
                          SDL_WINDOWPOS_CENTERED_DISPLAY(disp < 0 ? 0 : disp));
}

static void opt_change(int i, int dir)
{
    int *f = opt_field(i), c = opt_count(i);
    *f = (*f + dir + c) % c;
    if (i == OPT_SIZE) apply_window();
    options_save();
}

/* Durée en secondes, avec la virgule ou le point selon la langue : "1,2 s", "10 s". */
static void fmt_seconds(char *buf, size_t n, int ms)
{
    if (ms % 1000 == 0) snprintf(buf, n, "%d s", ms / 1000);
    else snprintf(buf, n, "%d%c%d s", ms / 1000, opt.lang == LANG_EN ? '.' : ',', ms % 1000 / 100);
}

static void opt_value(int i, char *buf, size_t n)
{
    switch (i) {
    case OPT_SIZE:
        if (opt.size >= N_SIZES - 1) snprintf(buf, n, "%s", T(S_FULLSCREEN));
        else snprintf(buf, n, "%d x %d", SIZES[opt.size][0], SIZES[opt.size][1]);
        break;
    case OPT_SPEED:
        if (opt.speed == 0) snprintf(buf, n, "%s", T(S_AUTO));
        else fmt_seconds(buf, n, SPEEDS[opt.speed]);
        break;
    case OPT_STEP:
        if (opt.step == 0) snprintf(buf, n, "%s", T(S_AUTO));
        else snprintf(buf, n, "+/- %d", STEPS[opt.step]);
        break;
    case OPT_MAX:   snprintf(buf, n, "%d", MAXES[opt.maxi]); break;
    case OPT_LANG:  snprintf(buf, n, "%s", LANG_NAMES[opt.lang]); break;
    case OPT_NEG:   snprintf(buf, n, "%s", T(opt.neg ? S_YES : S_NO)); break;
    default:        snprintf(buf, n, "%s", T(opt.music ? S_ON : S_OFF)); break;
    }
}

/* Réglages effectifs : l'option si elle est fixée, sinon la difficulté. */
static int eff_ms(int diff)   { return opt.speed ? SPEEDS[opt.speed] : DIFFICULTIES[diff].ms; }
static int eff_step(int diff) { return opt.step ? STEPS[opt.step] : DIFFICULTIES[diff].max_step; }

/* ---------- génération d'une manche ---------- */

static int rnd(int n) { return rand() % n; }

/* Symbole au hasard parmi ceux présents (present=1) ou absents (present=0). */
static int pick_sym(const int *val, int present)
{
    int c[N_SYMS], n = 0;
    for (int s = 0; s < N_SYMS; s++)
        if ((val[s] >= 0) == present) c[n++] = s;
    return c[rnd(n)];
}

static void new_round(void)
{
    const Difficulty *d = &DIFFICULTIES[sel_diff];

    memset(&rd, 0, sizeof rd);
    rd.mode = sel_mode;
    rd.diff = sel_diff;
    rd.nops = d->ops;
    rd.ms = eff_ms(sel_diff);
    int max_step = eff_step(sel_diff);

    if (rd.mode == MODE_NUM) {
        rd.hi = MAXES[opt.maxi];
        rd.lo = opt.neg ? -rd.hi : 1;
        /* départ entre max/10 et max/2 (10 à 50 pour un maximum de 100) */
        int v = rd.hi / 10 + rnd(rd.hi / 2 - rd.hi / 10 + 1);
        rd.start = v;
        for (int i = 0; i < rd.nops; i++) {
            int up = rd.hi - v, down = v - rd.lo;
            int step = 1 + rnd(max_step);
            int add = rnd(2);
            /* on garde le résultat entre lo et hi */
            if (add && step > up) add = 0;
            if (!add && step > down) add = 1;
            if (add && step > up) { /* trop grand dans les deux sens */
                add = up >= down;
                step = 1 + rnd(add ? up : down);
            }
            v += add ? step : -step;
            rd.ops[i].kind = add ? OP_ADD : OP_SUB;
            rd.ops[i].value = step;
        }
        rd.answer = v;
    } else if (rd.mode == MODE_CLOCK) {
        int h = rnd(24);
        rd.start = h;
        for (int i = 0; i < rd.nops; i++) {
            int step = 1 + rnd(max_step);
            int add = rnd(2);
            h += add ? step : -step;
            rd.ops[i].kind = add ? OP_ADD : OP_SUB;
            rd.ops[i].value = step;
            rd.ops[i].flavor = rnd(N_FLAVOR);
        }
        rd.answer = ((h % 24) + 24) % 24;
        rd.lo = 0;
        rd.hi = 23;
    } else {
        int val[N_SYMS];
        for (int s = 0; s < N_SYMS; s++) val[s] = -1;

        for (int i = 0; i < d->start_dice; i++) {
            int s = pick_sym(val, 0);
            val[s] = 1 + rnd(d->faces);
            rd.dice[rd.ndice].sym = s;
            rd.dice[rd.ndice].value = val[s];
            rd.ndice++;
        }

        int count = rd.ndice;
        for (int i = 0; i < rd.nops; i++) {
            int add = count <= 1 ? 1 : count >= N_SYMS ? 0 : rnd(100) < 55;
            int s = pick_sym(val, !add);
            if (add) {
                val[s] = 1 + rnd(d->faces);
                count++;
            } else {
                count--;
            }
            rd.ops[i].kind = add ? OP_DIE_ADD : OP_DIE_DEL;
            rd.ops[i].sym = s;
            rd.ops[i].value = val[s];
            if (!add) val[s] = -1;
        }

        rd.answer = 0;
        for (int s = 0; s < N_SYMS; s++) {
            if (val[s] < 0) continue;
            rd.answer += val[s];
            rd.final_dice[rd.nfinal].sym = s;
            rd.final_dice[rd.nfinal].value = val[s];
            rd.nfinal++;
        }
        rd.lo = 1;
        rd.hi = N_SYMS * d->faces;
    }
}

static void start_game(void)
{
    streak = 0;
    new_round();
    go(ST_INTRO);
}

static void to_menu(void)
{
    streak = 0;
    go(ST_MENU);
}

static void end_round(void)
{
    int *bt = &best_tries[rd.mode][rd.diff];
    int *bs = &best_streak[rd.mode][rd.diff];

    new_record = 0;
    if (rd.found && (*bt == 0 || rd.tries < *bt)) {
        *bt = rd.tries;
        if (!serie) new_record = 1;
    }
    if (serie && rd.found) {
        streak++;
        if (streak > *bs) {
            *bs = streak;
            new_record = 1;
        }
    }
    records_save();
    go(ST_RESULT);
}

static void submit_guess(void)
{
    if (rd.ninput == 0) return;
    int g = rd.negative ? -atoi(rd.input) : atoi(rd.input);
    rd.ninput = 0;
    rd.input[0] = '\0';
    rd.negative = 0;

    if (g < rd.lo || g > rd.hi) {
        set_msg(T(S_ANSWER_RANGE), rd.lo, rd.hi);
        return;
    }
    rd.tries++;
    if (rd.nhist < MAX_HIST) rd.hist[rd.nhist++] = g;

    if (g == rd.answer) {
        rd.found = 1;
        end_round();
    } else if (serie) {
        rd.found = 0;
        end_round();
    }
}

static void type_digit(char c)
{
    if (rd.ninput >= 3) return;
    if (rd.ninput == 1 && rd.input[0] == '0') rd.ninput = 0; /* pas de "07" */
    rd.input[rd.ninput++] = c;
    rd.input[rd.ninput] = '\0';
}

static void erase_digit(void)
{
    if (rd.ninput > 0) rd.input[--rd.ninput] = '\0';
    else rd.negative = 0;
}

static void toggle_sign(void)
{
    if (rd.lo < 0) rd.negative = !rd.negative;
}

static void next_after_result(void)
{
    if (serie && !rd.found) streak = 0;
    new_round();
    go(ST_INTRO);
}

/* ---------- entrées ---------- */

static int is_confirm(SDL_Keycode k)
{
    return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

static void on_key(SDL_Keycode k)
{
    switch (st) {
    case ST_MENU:
        if (k == SDLK_LEFT)  sel_mode = (sel_mode + N_MODES - 1) % N_MODES;
        if (k == SDLK_RIGHT) sel_mode = (sel_mode + 1) % N_MODES;
        if (k == SDLK_UP)    sel_diff = (sel_diff + N_DIFF - 1) % N_DIFF;
        if (k == SDLK_DOWN)  sel_diff = (sel_diff + 1) % N_DIFF;
        if (k == SDLK_s)     serie = !serie;
        if (k == SDLK_o)     go(ST_OPTIONS);
        if (is_confirm(k))   start_game();
        if (k == SDLK_ESCAPE) running = 0;
        break;
    case ST_OPTIONS:
        if (k == SDLK_UP)    opt_sel = (opt_sel + N_OPTS - 1) % N_OPTS;
        if (k == SDLK_DOWN)  opt_sel = (opt_sel + 1) % N_OPTS;
        if (k == SDLK_LEFT)  opt_change(opt_sel, -1);
        if (k == SDLK_RIGHT || is_confirm(k)) opt_change(opt_sel, 1);
        if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) go(ST_MENU);
        break;
    case ST_INTRO:
        if (is_confirm(k)) go(ST_COUNT);
        if (k == SDLK_ESCAPE) to_menu();
        break;
    case ST_COUNT:
    case ST_FLASH:
        if (k == SDLK_ESCAPE) to_menu();
        break;
    case ST_GUESS:
        if (k == SDLK_BACKSPACE) erase_digit();
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) submit_guess();
        if (k == SDLK_ESCAPE) to_menu();
        break;
    case ST_RESULT:
        if (is_confirm(k)) next_after_result();
        if (k == SDLK_ESCAPE) to_menu();
        break;
    }
}

static void update(void)
{
    Uint32 el = now - st_t0;
    if (st == ST_COUNT && el >= 3 * COUNT_MS) {
        go(ST_FLASH);
    } else if (st == ST_FLASH) {
        Uint32 period = (Uint32)(rd.ms + GAP_MS);
        if (el / period >= (Uint32)rd.nops) go(ST_GUESS);
    }
}

/* ---------- widgets ---------- */

static int hovered(int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

/* Bouton cliquable, retourne 1 au clic. */
static int button(int x, int y, int w, int h, const char *label, int sc, int active)
{
#ifdef TEXT_CHECK
    if (text_width(label, sc) > w - 8) printf("Texte trop long pour le bouton : %s\n", label);
#endif
    int hov = hovered(x, y, w, h);
    fill(x + 4, y + 4, w, h, (SDL_Color){ 0, 0, 0, 255 });
    fill(x, y, w, h, hov ? HOVER : PANEL);
    frame_rect(x, y, w, h, 3, active ? AMBER : hov ? CYAN : DIM);
    text_c(x + w / 2, y + (h - 7 * sc) / 2, sc, active ? AMBER : WHITE, "%s", label);
    if (hov && clicked) {
        clicked = 0;
        return 1;
    }
    return 0;
}

static int blink(int period_ms) { return (now / (Uint32)period_ms) % 2 == 0; }

static void draw_background(void)
{
    set_col(BG);
    SDL_RenderClear(R);
    for (int x = 0; x < W; x += 40) fill(x, 0, 1, H, GRID);
    for (int y = 0; y < H; y += 40) fill(0, y, W, 1, GRID);
}

static void draw_scanlines(void)
{
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(R, 0, 0, 0, 55);
    for (int y = 0; y < H; y += 3) SDL_RenderDrawLine(R, 0, y, W, y);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
}

static void draw_header(void)
{
    const char *mode = T(S_MODE_UP0 + rd.mode), *diff = T(S_DIFF_UP0 + rd.diff);
    if (serie)
        text_c(W / 2, 24, 3, CYAN, "%s - %s - %s : %d", mode, diff, T(S_HEADER_STREAK), streak);
    else
        text_c(W / 2, 24, 3, CYAN, "%s - %s", mode, diff);
}

static void format_answer(char *buf, size_t n, int mode, int v)
{
    if (mode == MODE_CLOCK) snprintf(buf, n, T(S_CLOCK_FMT), v);
    else snprintf(buf, n, "%d", v);
}

static const char *tries_word(int n) { return T(n == 1 ? S_TRY1 : S_TRYN); }

/* ---------- écrans ---------- */

static void draw_menu(void)
{
    text_c(W / 2 + 4, 34, 7, SHADOW, "GUESS THE NUMBER");
    text_c(W / 2, 30, 7, AMBER, "GUESS THE NUMBER");
    text_c(W / 2, 94, 2, PINK, "%s", T(S_SUBTITLE));

    text_l(60, 132, 2, CYAN, "%s", T(S_MODE_LABEL));
    for (int i = 0; i < N_MODES; i++) {
        int x = 60 + i * 290;
        if (button(x, 155, 260, 56, T(S_MODE0 + i), 4, sel_mode == i)) sel_mode = i;
        text_c(x + 130, 222, 2, DIM, "%s", T(S_SKILL0 + i));
    }

    text_l(60, 258, 2, CYAN, "%s", T(S_DIFF_LABEL));
    for (int i = 0; i < N_DIFF; i++) {
        int x = 60 + i * 290;
        if (button(x, 281, 260, 56, T(S_DIFF0 + i), 4, sel_diff == i)) sel_diff = i;
        int bt = best_tries[sel_mode][i], bs = best_streak[sel_mode][i];
        if (bt > 0) text_c(x + 130, 348, 2, WHITE, T(S_RECORD), bt, tries_word(bt));
        else        text_c(x + 130, 348, 2, DIM, "%s", T(S_RECORD_NONE));
        text_c(x + 130, 368, 2, bs > 0 ? WHITE : DIM, T(S_BEST_STREAK), bs);
    }

    char serie_label[48];
    snprintf(serie_label, sizeof serie_label, "[%c] %s", serie ? 'X' : ' ', T(S_SERIE_MODE));
    if (button(W / 2 - 200, 402, 400, 50, serie_label, 3, serie))
        serie = !serie;
    text_c(W / 2, 462, 2, DIM, "%s", T(serie ? S_NOTE_SERIE : S_NOTE_NORMAL));

    if (button(W / 2 - 140, 494, 280, 58, T(S_PLAY), 5, 1)) start_game();
    if (button(30, 504, 120, 40, T(S_OPTIONS_BTN), 2, 0)) go(ST_OPTIONS);
    if (button(W - 150, 504, 120, 40, T(S_QUIT), 2, 0)) running = 0;

    text_c(W / 2, 572, 2, DIM, "%s", T(S_MENU_HINT));
}

static void draw_options(void)
{
    text_c(W / 2 + 4, 34, 6, SHADOW, "%s", T(S_OPTIONS_TITLE));
    text_c(W / 2, 30, 6, AMBER, "%s", T(S_OPTIONS_TITLE));

    for (int i = 0; i < N_OPTS; i++) {
        int y = 92 + i * 54, sel = opt_sel == i;
        char val[32];
        opt_value(i, val, sizeof val);

        fill(60, y, 840, 46, sel ? HOVER : PANEL);
        frame_rect(60, y, 840, 46, 3, sel ? AMBER : DIM);
        text_l(80, y + 13, 3, sel ? AMBER : WHITE, "%s", T(S_OPT0 + i));
        if (button(560, y + 5, 40, 36, "<", 3, 0)) { opt_sel = i; opt_change(i, -1); }
        text_c(725, y + 13, 3, WHITE, "%s", val);
        if (button(850, y + 5, 40, 36, ">", 3, 0)) { opt_sel = i; opt_change(i, 1); }
        if (clicked && hovered(60, y, 840, 46)) {
            clicked = 0;
            opt_sel = i;
        }
    }

    text_c(W / 2, 474, 2, CYAN, "%s", T(S_HELP0 + opt_sel));

    if (button(W / 2 - 300, 505, 280, 48, T(S_DEFAULTS), 3, 0)) {
        /* on garde la langue : la remettre en français surprendrait */
        int size_changed = opt.size != OPT_DEFAULT.size, lang = opt.lang;
        opt = OPT_DEFAULT;
        opt.lang = lang;
        if (size_changed) apply_window();
        options_save();
    }
    if (button(W / 2 + 20, 505, 280, 48, T(S_BACK), 3, 1)) go(ST_MENU);

    text_c(W / 2, 572, 2, DIM, "%s", T(S_OPT_HINT));
}

static void draw_intro(void)
{
    const Difficulty *d = &DIFFICULTIES[rd.diff];
    draw_header();

    if (rd.mode == MODE_NUM) {
        text_c(W / 2, 110, 3, WHITE, "%s", T(S_START_NUM));
        text_c(W / 2, 160, 16, AMBER, "%d", rd.start);
    } else if (rd.mode == MODE_CLOCK) {
        char start[16];
        format_answer(start, sizeof start, MODE_CLOCK, rd.start);
        text_c(W / 2, 110, 3, WHITE, "%s", T(S_START_CLOCK));
        text_c(W / 2, 160, 14, AMBER, "%s", start);
        text_c(W / 2, 285, 2, DIM, "%s", T(S_CLOCK_WARMUP));
    } else {
        text_c(W / 2, 100, 3, WHITE, T(S_START_DICE), d->faces);
        draw_dice_row(rd.dice, rd.ndice, 150, 110);
        text_c(W / 2, 290, 2, PINK, "%s", T(S_REMEMBER));
    }

    char secs[16];
    fmt_seconds(secs, sizeof secs, rd.ms);
    text_c(W / 2, 350, 3, WHITE, T(S_OPS_COMING), rd.nops);
    text_c(W / 2, 390, 2, DIM, T(S_PER_OP), secs);
    if (rd.mode == MODE_NUM)
        text_c(W / 2, 420, 2, DIM, T(S_RANGE_NUM), rd.lo, rd.hi);
    else if (rd.mode == MODE_CLOCK)
        text_c(W / 2, 420, 2, DIM, "%s", T(S_GIVE_CLOCK));
    else if (rd.mode == MODE_DICE)
        text_c(W / 2, 420, 2, DIM, "%s", T(S_GIVE_DICE));

    if (blink(450)) text_c(W / 2, 480, 3, AMBER, "%s", T(S_CLICK_START));
    text_c(W / 2, 572, 2, DIM, "%s", T(S_ESC_MENU));

    if (clicked) {
        clicked = 0;
        go(ST_COUNT);
    }
}

static void draw_count(void)
{
    int n = 3 - (int)((now - st_t0) / COUNT_MS);
    if (n < 1) n = 1;
    draw_header();
    text_c(W / 2, 120, 3, DIM, "%s", T(S_READY));
    text_c(W / 2, 200, 28, PINK, "%d", n);
}

static void draw_flash(void)
{
    Uint32 el = now - st_t0;
    Uint32 period = (Uint32)(rd.ms + GAP_MS);
    int idx = (int)(el / period);
    if (idx >= rd.nops) return;

    draw_header();
    text_c(W / 2, 62, 2, DIM, "%d / %d", idx + 1, rd.nops);

    /* écran vide très court : sinon deux "+ 5" d'affilée se confondent */
    if (el % period >= (Uint32)rd.ms) return;

    const Op *o = &rd.ops[idx];
    SDL_Color c = idx % 2 ? AMBER : CYAN;

    if (rd.mode == MODE_NUM) {
        text_c(W / 2, 220, 22, c, "%c %d", o->kind == OP_ADD ? '+' : '-', o->value);
    } else if (rd.mode == MODE_CLOCK) {
        int fwd = o->kind == OP_ADD;
        text_c(W / 2, 170, 22, c, "%c%dh", fwd ? '+' : '-', o->value);
        text_c(W / 2, 400, 3, WHITE, "%s", T((fwd ? S_FUT0 : S_PAST0) + o->flavor));
        text_c(W / 2, 445, 2, DIM, "%s", T(fwd ? S_JUMP_FUTURE : S_JUMP_PAST));
    } else {
        int add = o->kind == OP_DIE_ADD;
        text_l(W / 2 - 250, 200, 22, add ? GREEN : RED, "%c", add ? '+' : '-');
        draw_die(W / 2 - 70, 150, 180, o->sym, add ? o->value : -1);
        text_c(W / 2, 400, 3, WHITE, T(add ? S_DIE_ADD : S_DIE_DEL), T(S_SYM0 + o->sym));
    }
}

static const char *KEYPAD[12] = { "7", "8", "9", "4", "5", "6", "1", "2", "3", "<", "0", "OK" };

static void draw_guess(void)
{
    draw_header();

    text_c(W / 2, 68, 3, WHITE, "%s", T(S_Q_NUM + rd.mode));
    text_c(W / 2, 100, 2, DIM, T(S_BETWEEN), rd.lo, rd.hi);

    /* champ de saisie */
    int bx = W / 2 - 150, by = 125;
    fill(bx, by, 300, 90, PANEL);
    frame_rect(bx, by, 300, 90, 3, AMBER);
    char shown[16];
    snprintf(shown, sizeof shown, "%s%s%s%s", rd.negative ? "-" : "", rd.input,
             blink(400) ? "_" : " ", rd.mode == MODE_CLOCK ? T(S_CLOCK_SUFFIX) : "");
    text_c(W / 2, by + 14, 9, WHITE, "%s", shown);

    /* touche moins, seulement si le résultat peut être négatif */
    if (rd.lo < 0 && button(W / 2 - 225, 235 + 3 * 68, 80, 58, "-", 4, rd.negative))
        toggle_sign();

    /* pavé numérique cliquable */
    int kx = W / 2 - 135, ky = 235;
    for (int i = 0; i < 12; i++) {
        int x = kx + (i % 3) * 90, y = ky + (i / 3) * 68;
        if (button(x, y, 80, 58, KEYPAD[i], 4, i == 11)) {
            if (i == 9) erase_digit();
            else if (i == 11) submit_guess();
            else type_digit(KEYPAD[i][0]);
            if (st != ST_GUESS) return;
        }
    }

    /* compteur d'essais */
    text_l(50, 140, 2, DIM, "%s", T(S_TRIES_LABEL));
    text_l(50, 165, 8, AMBER, "%d", rd.tries);
    if (serie) {
        text_l(50, 250, 2, PINK, "%s", T(S_ONE_TRY));
        text_l(50, 275, 2, DIM, T(S_STREAK_FMT), streak);
    } else if (rd.nhist > 0) {
        int more = rd.hist[rd.nhist - 1] < rd.answer;
        StrId hint = rd.mode == MODE_CLOCK ? (more ? S_BIG_LATER : S_BIG_EARLIER)
                                           : (more ? S_BIG_MORE : S_BIG_LESS);
        text_l(50, 260, 3, more ? CYAN : PINK, "%s", T(hint));
    }

    /* historique, le plus récent en haut */
    text_l(670, 140, 2, DIM, "%s", T(S_HISTORY));
    for (int i = 0; i < rd.nhist && i < 10; i++) {
        int g = rd.hist[rd.nhist - 1 - i];
        int more = g < rd.answer;
        char a[16];
        if (rd.mode == MODE_CLOCK) snprintf(a, sizeof a, T(S_CLOCK_SHORT), g);
        else snprintf(a, sizeof a, "%d", g);
        StrId hint = rd.mode == MODE_CLOCK ? (more ? S_LATER : S_EARLIER) : (more ? S_MORE : S_LESS);
        SDL_Color c = i == 0 ? (more ? CYAN : PINK) : DIM;
        text_l(670, 170 + i * 30, 3, c, "%-3s %s", a, T(hint));
    }

    if (msg[0] && now - msg_t < 2000) text_c(W / 2, 520, 2, RED, "%s", msg);
    text_c(W / 2, 572, 2, DIM, "%s", T(rd.lo < 0 ? S_GUESS_HINT_NEG : S_GUESS_HINT));
}

static void draw_result(void)
{
    char ans[16];
    format_answer(ans, sizeof ans, rd.mode, rd.answer);
    draw_header();

    if (rd.found) text_c(W / 2, 98, 10, GREEN, "%s", T(serie ? S_PERFECT : S_FOUND));
    else          text_c(W / 2, 98, 10, RED, "%s", T(S_MISSED));

    if (serie && !rd.found) {
        char last[16];
        format_answer(last, sizeof last, rd.mode, rd.hist[rd.nhist - 1]);
        text_c(W / 2, 188, 3, WHITE, T(S_WAS_ANSWERED), ans, last);
    } else if (serie) {
        text_c(W / 2, 188, 3, WHITE, T(S_WAS_INDEED), ans);
    } else {
        text_c(W / 2, 188, 3, WHITE, T(S_WAS_TRIES), ans, rd.tries, tries_word(rd.tries));
    }

    if (rd.mode == MODE_DICE) {
        text_c(W / 2, 226, 2, DIM, "%s", T(S_DICE_LEFT));
        draw_dice_row(rd.final_dice, rd.nfinal, 248, 60);
    }

    StrId comment;
    if (serie && rd.found)   comment = streak > 1 ? S_STREAK_GOES : S_FIRST_ROUND;
    else if (serie)          comment = S_STREAK_OVER;
    else if (rd.tries == 1)  comment = S_COMMENT1;
    else if (rd.tries <= 3)  comment = S_COMMENT3;
    else                     comment = S_COMMENTN;
    text_c(W / 2, 326, 3, AMBER, "%s", T(comment));

    if (serie)
        text_c(W / 2, 358, 2, WHITE, T(S_ROUNDS_WON), streak, best_streak[rd.mode][rd.diff]);

    if (new_record && blink(350))
        text_c(W / 2, 395, 4, PINK, "%s", T(S_NEW_RECORD));

    const char *next = T(!serie ? S_REPLAY : rd.found ? S_NEXT_ROUND : S_NEW_STREAK);
    if (button(W / 2 - 300, 450, 290, 60, next, 3, 1)) { next_after_result(); return; }
    if (button(W / 2 + 10, 450, 290, 60, T(S_MENU), 3, 0)) { to_menu(); return; }

    text_c(W / 2, 572, 2, DIM, T(S_RESULT_HINT), next);
}

/* ---------- boucle principale ---------- */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Guess the Number", SDL_GetError(), NULL);
        return 1;
    }
    win = SDL_CreateWindow("Guess the Number 19XX", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_RESIZABLE);
    if (!win) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Guess the Number", SDL_GetError(), NULL);
        SDL_Quit();
        return 1;
    }
    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!R) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Guess the Number", SDL_GetError(), win);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(R, W, H);
    SDL_StartTextInput();

    font_init();
    srand((unsigned)time(NULL));
    records_load();
    options_load();
    if (opt.size != 0) apply_window();
    go(ST_MENU);

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        SDL_Event e;
        clicked = 0;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_MOUSEMOTION:
                mx = e.motion.x;
                my = e.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mx = e.button.x;
                    my = e.button.y;
                    clicked = 1;
                }
                break;
            case SDL_TEXTINPUT:
                /* chiffres via le texte : marche aussi en AZERTY et au pavé numérique */
                if (st == ST_GUESS) {
                    for (const char *p = e.text.text; *p; p++) {
                        if (*p >= '0' && *p <= '9') type_digit(*p);
                        else if (*p == '-') toggle_sign();
                    }
                }
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat && e.key.keysym.sym != SDLK_BACKSPACE) break;
                on_key(e.key.keysym.sym);
                break;
            }
        }

        now = SDL_GetTicks();
        update();

        draw_background();
        switch (st) {
        case ST_MENU:    draw_menu();    break;
        case ST_OPTIONS: draw_options(); break;
        case ST_INTRO:  draw_intro();  break;
        case ST_COUNT:  draw_count();  break;
        case ST_FLASH:  draw_flash();  break;
        case ST_GUESS:  draw_guess();  break;
        case ST_RESULT: draw_result(); break;
        }
        draw_scanlines();
        SDL_RenderPresent(R);

        /* au cas où la synchro verticale est désactivée */
        Uint32 spent = SDL_GetTicks() - frame_start;
        if (spent < 8) SDL_Delay(8 - spent);
    }

    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
