/*
 * tetris.h — Definições e constantes do Tetris para DE10
 */

#ifndef TETRIS_H
#define TETRIS_H

#include <stdint.h>
#include "video.h"

/* ============================================================
 *  Configurações do tabuleiro
 * ============================================================ */
#define BOARD_COLS       10
#define BOARD_ROWS       20
#define CELL_SIZE        10    /* Tamanho de cada célula em pixels */
#define BOARD_OFFSET_X   10   /* Posição X do tabuleiro na tela */
#define BOARD_OFFSET_Y   10   /* Posição Y do tabuleiro na tela */

/* Área lateral (painéis de pontuação, próxima peça, etc.) */
#define PANEL_X         (BOARD_OFFSET_X + BOARD_COLS * CELL_SIZE + 8)
#define PANEL_Y          10

/* ============================================================
 *  Hardware da DE10 — endereços vêm do address_map_arm.h,
 *  já incluído indiretamente via video.h. Só criamos aliases
 *  para os nomes de display HEX (nomes diferentes no header).
 * ============================================================ */
#define HEX03_BASE       HEX3_HEX0_BASE   /* Display 7-seg HEX0-HEX3 */
#define HEX45_BASE       HEX5_HEX4_BASE   /* Display 7-seg HEX4-HEX5 */
/* KEY_BASE, SW_BASE, TIMER_BASE já vêm certos do address_map_arm.h */

/* Máscaras dos botões (ativo em 0 — pressionar = bit vai a 0) */
#define KEY0_MASK        0x01   /* Mover direita     */
#define KEY1_MASK        0x02   /* Mover esquerda    */
#define KEY2_MASK        0x04   /* Rotacionar        */
#define KEY3_MASK        0x08   /* Drop rápido       */

/* Máscaras dos switches */
#define SW0_MASK         0x01   /* Pausar / retomar  */
#define SW1_MASK         0x02   /* Reset             */

/* ============================================================
 *  Configurações do jogo
 * ============================================================ */
#define NUM_PIECE_TYPES  7
#define PIECE_GRID_SIZE  4

/* Intervalo de queda (em ciclos de poll) — ajuste para sua velocidade preferida */
#define DROP_INTERVAL_START  15   /* Intervalo inicial (mais lento) */
#define DROP_INTERVAL_MIN     3   /* Intervalo mínimo  (mais rápido) */

/* Carência (em ciclos extras) antes da queda automática começar após
 * cada peça nascer — dá tempo de ver/reagir à peça nova, e evita a
 * sensação de "a peça já caiu sozinha" logo no início do jogo. */
#define SPAWN_GRACE_TICKS     12

/* Pontuação por linhas eliminadas */
#define POINTS_1_LINE   100
#define POINTS_2_LINES  300
#define POINTS_3_LINES  500
#define POINTS_4_LINES  800    /* Tetris! */

/* ============================================================
 *  Tipos e structs
 * ============================================================ */

/* Estado de cada célula no tabuleiro: 0 = vazia, 1-7 = cor da peça */
typedef uint8_t Board[BOARD_ROWS][BOARD_COLS];

/* Representa uma peça ativa */
typedef struct {
    int type;               /* Tipo 0-6 (I, O, T, S, Z, J, L) */
    int rotation;           /* Rotação atual 0-3               */
    int x;                  /* Coluna da posição atual          */
    int y;                  /* Linha da posição atual           */
} Piece;

/* Estado global do jogo */
typedef struct {
    Board  board;           /* Tabuleiro fixo                   */
    Piece  current;         /* Peça em queda                    */
    Piece  next;            /* Próxima peça (preview)           */
    int    score;           /* Pontuação acumulada              */
    int    level;           /* Nível atual                      */
    int    lines_cleared;   /* Total de linhas eliminadas       */
    int    drop_timer;      /* Contador para controlar a queda  */
    int    drop_interval;   /* Intervalo de queda atual         */
    int    paused;          /* 1 = pausado                      */
    int    game_over;       /* 1 = jogo encerrado               */
} GameState;

/* ============================================================
 *  Definição dos tetrominós
 *
 *  Cada peça é uma grade 4×4 de bits (16 bits por rotação).
 *  Bit 15 = canto superior-esquerdo, bit 0 = canto inferior-direito.
 *
 *  Formato:
 *    pieces[tipo][rotação] = máscara de 16 bits
 *
 *  Para decodificar a célula (row, col):
 *    bit_index = row * 4 + col
 *    ocupado = (mascara >> (15 - bit_index)) & 1
 * ============================================================ */

/* Cores de cada tipo de peça (índice 0-6) */
static const int piece_colors[NUM_PIECE_TYPES] = {
    COLOR_PIECE_I,   /* 0 — I */
    COLOR_PIECE_O,   /* 1 — O */
    COLOR_PIECE_T,   /* 2 — T */
    COLOR_PIECE_S,   /* 3 — S */
    COLOR_PIECE_Z,   /* 4 — Z */
    COLOR_PIECE_J,   /* 5 — J */
    COLOR_PIECE_L,   /* 6 — L */
};

/*
 * Máscara de 16 bits para cada rotação de cada peça.
 * Cada linha abaixo: [tipo][4 rotações]
 *
 * Legenda visual:
 *   1 = bloco presente, 0 = vazio
 *   Grade 4×4, linha-maior
 */
static const uint16_t pieces[NUM_PIECE_TYPES][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, /* I */
    {0x0660, 0x0660, 0x0660, 0x0660}, /* O */
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, /* T */
    {0x06C0, 0x4620, 0x06C0, 0x4620}, /* S */
    {0x0C60, 0x2640, 0x0C60, 0x2640}, /* Z */
    {0x44C0, 0x8E00, 0xC880, 0x0E20}, /* J */
    {0x88C0, 0x0E80, 0xC220, 0x4E00}, /* L */
};

/* ============================================================
 *  Protótipos de funções
 * ============================================================ */

/* Inicialização */
void game_init(GameState *gs);

/* Loop principal */
void game_update(GameState *gs);

/* Lógica de movimento */
int  piece_can_move(const GameState *gs, int dx, int dy, int new_rotation);
void piece_move(GameState *gs, int dx, int dy);
void piece_rotate(GameState *gs);
void piece_drop(GameState *gs);       /* Drop rápido até o fundo        */
void piece_lock(GameState *gs);       /* Fixa a peça no tabuleiro       */
void piece_spawn(GameState *gs);      /* Gera próxima peça              */
int  piece_ghost_y(const GameState *gs); /* Linha do ghost (preview)    */

/* Lógica do tabuleiro */
int  board_check_lines(GameState *gs); /* Verifica e remove linhas cheias */
void board_clear(Board board);

/* Leitura de hardware */
int  read_keys(void);
int  read_switches(void);

/* Renderização */
void render_board(const GameState *gs);
void render_current_piece(const GameState *gs);
void render_ghost(const GameState *gs);
void render_next_piece(const GameState *gs);
void render_hud(const GameState *gs);
void render_game_over(void);
void render_pause(void);
void render_all(const GameState *gs);

/* Utilitários */
int  cell_occupied(int type, int rotation, int row, int col);
void score_add(GameState *gs, int lines);
int  level_from_lines(int lines);
void simple_delay(int n);

#endif /* TETRIS_H */