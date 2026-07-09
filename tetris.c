/*
 * tetris.c — Implementação completa do Tetris para DE10
 *
 * Contém:
 *   - Inicialização e loop principal do jogo
 *   - Lógica de movimento, rotação e colisão
 *   - Limpeza de linhas e pontuação
 *   - Leitura dos botões KEY e switches SW
 *   - Renderização via video.c (pixel buffer VGA)
 *   - Display 7-segmentos para pontuação
 *   - Função main
 */

#include "tetris.h"
#include "video.h"
#include <string.h>
#include <stdlib.h>  /* rand(), srand() */

/* ============================================================
 *  Display 7-segmentos
 *  Segmentos: gfedcba → bit 6..0 (ativo em 0 no hardware real,
 *  mas aqui usamos lógica positiva e invertemos ao escrever)
 * ============================================================ */
static const uint8_t seg7_digits[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F, /* 9 */
};

static volatile int *hex03 = (volatile int *)HEX03_BASE;
static volatile int *hex45 = (volatile int *)HEX45_BASE;

/* Escreve um número de 0–999999 nos displays HEX5–HEX0 */
static void display_score_hex(int score) {
    int d[6];
    int i;
    int v = score;
    for (i = 0; i < 6; i++) {
        d[i] = seg7_digits[v % 10];
        v /= 10;
    }
    *hex03 = (d[3] << 24) | (d[2] << 16) | (d[1] << 8) | d[0];
    *hex45 = (d[5] << 8) | d[4];
}

/* ============================================================
 *  Gerador de números aleatórios simples (LCG)
 *  Semeado com leitura de timer para variar entre partidas
 * ============================================================ */
static volatile int *timer_reg = (volatile int *)TIMER_BASE;
static unsigned int rand_seed = 12345;

static int rng_next(int n) {
    rand_seed = rand_seed * 1664525u + 1013904223u;
    return (int)((rand_seed >> 16) % (unsigned int)n);
}

/* Usa o contador do timer como semente (lê bit baixo do snapshot) */
static void rng_seed_from_timer(void) {
    rand_seed = (unsigned int)(*(timer_reg + 2));  /* low snapshot */
    if (rand_seed == 0) rand_seed = 0xDEADBEEF;
}

/* ============================================================
 *  Leitura de hardware
 * ============================================================ */
static volatile int *key_reg = (volatile int *)KEY_BASE;
static volatile int *sw_reg  = (volatile int *)SW_BASE;

int read_keys(void) {
    /*
     * KEY0-3 são ativo-baixo: pressionado = bit = 0.
     * Invertemos para que bit = 1 signifique "pressionado".
     * Lemos apenas os 4 bits inferiores.
     */
    return (~(*key_reg)) & 0x0F;
}

int read_switches(void) {
    return (*sw_reg) & 0x03;  /* SW0 e SW1 */
}

/* ============================================================
 *  Utilitário: retorna 1 se a célula (row, col) da grade 4×4
 *  da peça [type][rotation] está ocupada
 * ============================================================ */
int cell_occupied(int type, int rotation, int row, int col) {
    uint16_t mask = pieces[type][rotation];
    int bit = row * 4 + col;           /* 0 = canto sup-esq, 15 = inf-dir */
    return (mask >> (15 - bit)) & 1;
}

/* ============================================================
 *  Tabuleiro
 * ============================================================ */
void board_clear(Board board) {
    memset(board, 0, sizeof(Board));
}

/*
 * Verifica se há linhas completas, remove-as (empurrando as de cima
 * para baixo) e retorna quantas foram removidas.
 */
int board_check_lines(GameState *gs) {
    int row, col, r, full, count = 0;

    for (row = BOARD_ROWS - 1; row >= 0; row--) {
        full = 1;
        for (col = 0; col < BOARD_COLS; col++) {
            if (gs->board[row][col] == 0) { full = 0; break; }
        }
        if (full) {
            /* Remove a linha: desloca tudo acima uma linha para baixo manualmente */
            for (r = row; r > 0; r--) {
                for (col = 0; col < BOARD_COLS; col++) {
                    gs->board[r][col] = gs->board[r - 1][col];
                }
            }
            
            /* Linha 0 fica vazia - limpa manualmente */
            for (col = 0; col < BOARD_COLS; col++) {
                gs->board[0][col] = 0;
            }

            count++;
            row++;   /* Re-verifica a mesma posição após o shift */
        }
    }
    return count;
}

/* ============================================================
 *  Pontuação e nível
 * ============================================================ */
int level_from_lines(int lines) {
    return lines / 10 + 1;
}

void score_add(GameState *gs, int lines) {
    switch (lines) {
        case 1: gs->score += POINTS_1_LINE   * gs->level; break;
        case 2: gs->score += POINTS_2_LINES  * gs->level; break;
        case 3: gs->score += POINTS_3_LINES  * gs->level; break;
        case 4: gs->score += POINTS_4_LINES  * gs->level; break;
        default: break;
    }
    gs->lines_cleared += lines;
    gs->level = level_from_lines(gs->lines_cleared);

    /* Atualiza velocidade de queda */
    gs->drop_interval = DROP_INTERVAL_START - (gs->level - 1) * 3;
    if (gs->drop_interval < DROP_INTERVAL_MIN)
        gs->drop_interval = DROP_INTERVAL_MIN;
}

/* ============================================================
 *  Lógica das peças
 * ============================================================ */

/*
 * Verifica se a peça atual, com deslocamento (dx, dy) e
 * rotação new_rotation, não colide com paredes nem células fixas.
 * Retorna 1 se o movimento é válido, 0 caso contrário.
 */
int piece_can_move(const GameState *gs, int dx, int dy, int new_rotation) {
    int row, col;
    int nx, ny;
    const Piece *p = &gs->current;

    for (row = 0; row < PIECE_GRID_SIZE; row++) {
        for (col = 0; col < PIECE_GRID_SIZE; col++) {
            if (!cell_occupied(p->type, new_rotation, row, col))
                continue;

            nx = p->x + col + dx;
            ny = p->y + row + dy;

            /* Fora dos limites horizontais */
            if (nx < 0 || nx >= BOARD_COLS) return 0;
            /* Abaixo do fundo */
            if (ny >= BOARD_ROWS) return 0;
            /* Acima da tela (permitido durante spawn) */
            if (ny < 0) continue;
            /* Colisão com célula fixa */
            if (gs->board[ny][nx] != 0) return 0;
        }
    }
    return 1;
}

void piece_move(GameState *gs, int dx, int dy) {
    if (piece_can_move(gs, dx, dy, gs->current.rotation)) {
        gs->current.x += dx;
        gs->current.y += dy;
    }
}

void piece_rotate(GameState *gs) {
    int new_rot = (gs->current.rotation + 1) % 4;

    /* Tenta rotacionar na posição atual */
    if (piece_can_move(gs, 0, 0, new_rot)) {
        gs->current.rotation = new_rot;
        return;
    }
    
    /* Só tenta o wall-kick (deslocamento lateral) se a peça NÃO estiver saindo pelo topo (y < 0),
     * evitando que ela trave, suma ou quebre os limites superiores ao girar. */
    if (gs->current.y >= 0) {
        /* Wall-kick: tenta deslocar 1 célula para a direita */
        if (piece_can_move(gs, 1, 0, new_rot)) {
            gs->current.x++;
            gs->current.rotation = new_rot;
            return;
        }
        /* Wall-kick: tenta deslocar 1 célula para a esquerda */
        if (piece_can_move(gs, -1, 0, new_rot)) {
            gs->current.x--;
            gs->current.rotation = new_rot;
            return;
        }
    }
}

/* Drop rápido: move a peça para baixo até não poder mais */
void piece_drop(GameState *gs) {
    while (piece_can_move(gs, 0, 1, gs->current.rotation))
        gs->current.y++;
    piece_lock(gs);
}

/* Fixa a peça no tabuleiro e gera próxima peça */
void piece_lock(GameState *gs) {
    int row, col;
    Piece *p = &gs->current;
    int ny, nx;
    int lines;

    for (row = 0; row < PIECE_GRID_SIZE; row++) {
        for (col = 0; col < PIECE_GRID_SIZE; col++) {
            if (!cell_occupied(p->type, p->rotation, row, col)) continue;
            ny = p->y + row;
            nx = p->x + col;
            if (ny < 0 || ny >= BOARD_ROWS || nx < 0 || nx >= BOARD_COLS) continue;
            /* Armazena tipo+1 para distinguir de vazio (0) */
            gs->board[ny][nx] = (uint8_t)(p->type + 1);
        }
    }

    lines = board_check_lines(gs);
    if (lines > 0)
        score_add(gs, lines);

    display_score_hex(gs->score);
    piece_spawn(gs);
}

/* Calcula a linha Y onde a peça cairia (ghost piece) */
int piece_ghost_y(const GameState *gs) {
    int ghost_y = gs->current.y;
    const Piece *p = &gs->current;
    int row, col, nx, ny;

    /* Desce até colidir */
    while (1) {
        /* Verifica se pode descer mais 1 */
        int can = 1;
        for (row = 0; row < PIECE_GRID_SIZE && can; row++) {
            for (col = 0; col < PIECE_GRID_SIZE && can; col++) {
                if (!cell_occupied(p->type, p->rotation, row, col)) continue;
                nx = p->x + col;
                ny = ghost_y + row + 1;
                if (ny >= BOARD_ROWS) { can = 0; break; }
                if (ny >= 0 && gs->board[ny][nx] != 0) { can = 0; break; }
            }
        }
        if (!can) break;
        ghost_y++;
    }
    return ghost_y;
}

/* Gera próxima peça (coloca "next" como "current" e sorteia nova "next") */
void piece_spawn(GameState *gs) {
    gs->current = gs->next;

    /* Posição inicial: centralizado horizontalmente, acima do tabuleiro */
    gs->current.x = BOARD_COLS / 2 - 2;
    gs->current.y = -1;
    gs->current.rotation = 0;

    /* Verifica se a posição inicial já colide → game over */
    if (!piece_can_move(gs, 0, 0, gs->current.rotation)) {
        gs->game_over = 1;
        return;
    }

    /* Carência antes da queda automática: drop_timer negativo significa
     * que faltam mais SPAWN_GRACE_TICKS ciclos além do drop_interval
     * normal para a primeira queda automática acontecer. Dá tempo de
     * ver a peça nova e reagir antes dela começar a cair sozinha. */
    gs->drop_timer = -SPAWN_GRACE_TICKS;

    /* Sorteia próxima peça */
    gs->next.type     = rng_next(NUM_PIECE_TYPES);
    gs->next.rotation = 0;
    gs->next.x        = 0;
    gs->next.y        = 0;
}

/* ============================================================
 *  Inicialização
 * ============================================================ */
void game_init(GameState *gs) {
    /* 1. Limpa absolutamente toda a estrutura de memória primeiro */
    memset(gs, 0, sizeof(GameState));
    
    /* 2. Garante explicitamente que a matriz do tabuleiro comece zerada */
    board_clear(gs->board);

    /* 3. Define as variáveis iniciais de estado */
    gs->score         = 0;
    gs->level         = 1;
    gs->lines_cleared = 0;
    gs->drop_timer    = 0;
    gs->drop_interval = DROP_INTERVAL_START;
    gs->paused        = 0;
    gs->game_over     = 0;

    /* 4. Semeia o gerador de números aleatórios */
    rng_seed_from_timer();

    /* 5. Sorteia a peça de preview e spawna a primeira peça ativa */
    gs->next.type     = rng_next(NUM_PIECE_TYPES);
    gs->next.rotation = 0;
    gs->next.x        = 0;
    gs->next.y        = 0;

    piece_spawn(gs);
    
    /* 6. Atualiza o display físico de pontuação */
    display_score_hex(0);
}

/* ============================================================
 *  Atraso simples (busy-wait)
 * ============================================================ */
void simple_delay(int n) {
    volatile int i;
    for (i = 0; i < n; i++)
        ;
}

/* ============================================================
 *  Renderização
 * ============================================================ */

/* Coluna/linha de caractere onde começa o painel lateral (HUD) */
#define PANEL_COL   15
#define PANEL_ROW    0

/* Renderiza o tabuleiro fixo */
void render_board(const GameState *gs) {
    int row, col;
    uint8_t cell;
    int color;
    int px, py;

    /* Fundo do tabuleiro */
    video_draw_rect(BOARD_OFFSET_X, BOARD_OFFSET_Y,
                    BOARD_COLS * CELL_SIZE, BOARD_ROWS * CELL_SIZE,
                    COLOR_DARK_GRAY);

    /* Borda do tabuleiro */
    video_draw_rect_outline(BOARD_OFFSET_X - 1, BOARD_OFFSET_Y - 1,
                            BOARD_COLS * CELL_SIZE + 2,
                            BOARD_ROWS * CELL_SIZE + 2,
                            COLOR_WHITE);

    /* Células fixas — sem borda 3D (custava 5x mais chamadas de
     * desenho por célula; com o tabuleiro quase cheio isso ficava
     * lento o bastante para parecer um travamento real). */
    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            cell = gs->board[row][col];
            if (cell == 0) continue;
            color = piece_colors[cell - 1];
            px = BOARD_OFFSET_X + col * CELL_SIZE;
            py = BOARD_OFFSET_Y + row * CELL_SIZE;
            video_draw_rect(px, py, CELL_SIZE, CELL_SIZE, color);
        }
    }
}

/* Renderiza a ghost piece (projeção de queda) */
void render_ghost(const GameState *gs) {
    int ghost_y, row, col, px, py;
    const Piece *p = &gs->current;

    ghost_y = piece_ghost_y(gs);

    /* Não desenha ghost se está na mesma posição que a peça atual */
    if (ghost_y == p->y) return;

    for (row = 0; row < PIECE_GRID_SIZE; row++) {
        for (col = 0; col < PIECE_GRID_SIZE; col++) {
            if (!cell_occupied(p->type, p->rotation, row, col)) continue;
            py = BOARD_OFFSET_Y + (ghost_y + row) * CELL_SIZE;
            px = BOARD_OFFSET_X + (p->x + col) * CELL_SIZE;
            if (py < BOARD_OFFSET_Y) continue;
            video_draw_rect_outline(px, py, CELL_SIZE, CELL_SIZE, COLOR_GHOST);
        }
    }
}

/* Renderiza a peça em queda */
void render_current_piece(const GameState *gs) {
    int row, col, px, py;
    int color;
    const Piece *p = &gs->current;

    color = piece_colors[p->type];

    for (row = 0; row < PIECE_GRID_SIZE; row++) {
        for (col = 0; col < PIECE_GRID_SIZE; col++) {
            if (!cell_occupied(p->type, p->rotation, row, col)) continue;
            py = BOARD_OFFSET_Y + (p->y + row) * CELL_SIZE;
            px = BOARD_OFFSET_X + (p->x + col) * CELL_SIZE;
            if (py < BOARD_OFFSET_Y) continue; /* Célula acima da tela */
            video_draw_rect(px, py, CELL_SIZE, CELL_SIZE, color);
            video_draw_rect_outline(px, py, CELL_SIZE, CELL_SIZE, COLOR_WHITE);
        }
    }
}

/* Renderiza preview da próxima peça no painel lateral */
void render_next_piece(const GameState *gs) {
    int row, col, px, py;
    int color;
    const Piece *p = &gs->next;
    int preview_x = PANEL_X;
    int preview_y = PANEL_Y + 20;

    /* Título (texto via hardware, coordenadas em células) */
    video_text(PANEL_COL, PANEL_ROW, "NEXT");

    /* Fundo da área de preview (4×4 células de jogo, em pixels) */
    video_draw_rect(preview_x, preview_y,
                    4 * CELL_SIZE, 4 * CELL_SIZE, COLOR_DARK_GRAY);
    video_draw_rect_outline(preview_x - 1, preview_y - 1,
                            4 * CELL_SIZE + 2, 4 * CELL_SIZE + 2,
                            COLOR_LIGHT_GRAY);

    color = piece_colors[p->type];

    for (row = 0; row < PIECE_GRID_SIZE; row++) {
        for (col = 0; col < PIECE_GRID_SIZE; col++) {
            if (!cell_occupied(p->type, 0, row, col)) continue;
            px = preview_x + col * CELL_SIZE;
            py = preview_y + row * CELL_SIZE;
            video_draw_rect(px, py, CELL_SIZE, CELL_SIZE, color);
            video_draw_rect_outline(px, py, CELL_SIZE, CELL_SIZE, COLOR_WHITE);
        }
    }
}

/*
 * Converte inteiro para string (sem depender de sprintf/itoa)
 * Escreve em buf (terminado em '\0'), retorna ponteiro para buf.
 */
static char *int_to_str(int v, char *buf, int buflen) {
    int i = buflen - 1;
    buf[i] = '\0';
    if (v == 0) {
        buf[--i] = '0';
    } else {
        while (v > 0 && i > 0) {
            buf[--i] = (char)('0' + v % 10);
            v /= 10;
        }
    }
    return &buf[i];
}

/* Renderiza HUD: score, nível, linhas (texto via hardware) */
void render_hud(const GameState *gs) {
    char buf[12];
    int row = PANEL_ROW + 10;  /* Abaixo da área de preview, com folga */
    int col = PANEL_COL;

    video_text(col, row,     "SCORE");
    video_text(col, row + 1, int_to_str(gs->score, buf, 12));

    video_text(col, row + 3, "LEVEL");
    video_text(col, row + 4, int_to_str(gs->level, buf, 12));

    video_text(col, row + 6, "LINES");
    video_text(col, row + 7, int_to_str(gs->lines_cleared, buf, 12));

    /* Legenda dos botões */
    video_text(col, row + 9,  "K0>");
    video_text(col, row + 10, "K1<");
    video_text(col, row + 11, "K2^");
    video_text(col, row + 12, "K3V");
}

/* Tela de game over */
void render_game_over(void) {
    /* Overlay: um retângulo escuro (pixels) com texto do hardware */
    video_draw_rect(40, 90, 240, 50, COLOR_DARK_BLUE);
    video_draw_rect_outline(39, 89, 242, 52, COLOR_RED);
    video_text(15, 13, "GAME OVER");
    video_text(15, 15, "SW1=RESET");
}

/* Tela de pause */
void render_pause(void) {
    video_draw_rect(55, 100, 210, 35, COLOR_DARK_BLUE);
    video_draw_rect_outline(54, 99, 212, 37, COLOR_YELLOW);
    video_text(14, 13, "-- PAUSED --");
    video_text(15, 15, "SW0=RESUME");
}

/* Renderização completa do frame */
void render_all(const GameState *gs) {
    video_clear(COLOR_BLACK);
    render_board(gs);
    if (!gs->game_over && !gs->paused)
        render_ghost(gs);
    render_current_piece(gs);
    render_next_piece(gs);
    render_hud(gs);
    if (gs->game_over) render_game_over();
    if (gs->paused)    render_pause();
    /* Sem swap manual: cada video_draw_rect já escreve direto no
     * framebuffer ativo (lido do registrador a cada chamada), na
     * mesma técnica comprovada do driver de referência. */
}

/* ============================================================
 *  Loop principal do jogo
 * ============================================================ */

/*
 * Debounce simples: armazena o estado anterior dos botões para
 * detectar borda de descida (transição 0→1) e evitar repetição.
 */
static int prev_keys = 0;
static int prev_sw   = 0;

/* Retorna 1 apenas no ciclo em que a tecla foi pressionada */
static int key_pressed(int current_keys, int mask) {
    return (current_keys & mask) && !(prev_keys & mask);
}

static int sw_changed(int current_sw, int mask) {
    return (current_sw & mask) && !(prev_sw & mask);
}

void game_update(GameState *gs) {
    int keys = read_keys();
    int sw   = read_switches();

    /* SW1 = Reset (qualquer estado) */
    if (sw_changed(sw, SW1_MASK)) {
        game_init(gs);
        prev_keys = 0;
        prev_sw   = sw;
        return;
    }

    /* SW0 = Pause/Resume (toggle na borda de subida) */
    if (sw_changed(sw, SW0_MASK)) {
        gs->paused = !gs->paused;
    }

    prev_sw = sw;

    if (gs->game_over || gs->paused) {
        prev_keys = keys;
        return;
    }

    /* Processar botões (detecção de borda) */
    if (key_pressed(keys, KEY0_MASK)) piece_move(gs, 1, 0);   /* Direita */
    if (key_pressed(keys, KEY1_MASK)) piece_move(gs, -1, 0);  /* Esquerda */
    if (key_pressed(keys, KEY2_MASK)) piece_rotate(gs);        /* Rotacionar */
    if (key_pressed(keys, KEY3_MASK)) piece_drop(gs);          /* Drop rápido */

    prev_keys = keys;

    /* Temporizador de queda automática */
    gs->drop_timer++;
    if (gs->drop_timer >= gs->drop_interval) {
        gs->drop_timer = 0;
        if (piece_can_move(gs, 0, 1, gs->current.rotation)) {
            gs->current.y++;
        } else {
            piece_lock(gs);
        }
    }
}

/* ============================================================
 *  Ponto de entrada
 * ============================================================ */
int main(void) {
    GameState gs;

    video_init();
    game_init(&gs);

    while (1) {
        game_update(&gs);
        render_all(&gs);

        /*
         * Pequeno atraso de frame. Reduzido em relação ao valor original,
         * já que o Nios II aqui compila sem multiplicador/divisor de
         * hardware (-mno-hw-mul -mno-hw-div) — o próprio render_all()
         * já consome tempo real considerável, então não precisamos de
         * um delay tão grande quanto se assumia antes.
         */
        simple_delay(150000);
    }

    return 0;  /* Nunca alcançado */
}