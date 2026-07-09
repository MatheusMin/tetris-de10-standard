/*
 * video.h — Driver de Pixel Buffer VGA para DE10-Standard
 *
 * Reescrito com base numa técnica de endereçamento CONFIRMADA como
 * funcional no hardware real (extraída de um Flappy Bird de referência
 * que renderiza corretamente nessa mesma placa/sistema).
 *
 * Diferenças principais em relação à versão anterior (que tinha bugs
 * de distorção/ruído na tela):
 *   1. O stride (bytes por linha) do framebuffer real é uma potência
 *      de 2 (tipicamente 1024 bytes/linha a 16-bit), NÃO simplesmente
 *      SCREEN_WIDTH * 2. Escrever assumindo o stride errado faz cada
 *      linha desenhada "vazar" para a linha seguinte errada — era
 *      exatamente a causa do ruído progressivo visto antes.
 *   2. O ponteiro do framebuffer é lido diretamente do registrador
 *      PIXEL_BUF_CTRL_BASE a cada desenho, em vez de usar um endereço
 *      de back buffer fixo — evita colisão/aliasing com a SDRAM onde
 *      o próprio programa roda.
 *   3. Texto usa o buffer de caracteres dedicado do hardware
 *      (FPGA_CHAR_BASE), não uma fonte 8×8 desenhada pixel a pixel.
 */

#ifndef VIDEO_H
#define VIDEO_H

#include "address_map_arm.h"

/* ============================================================
 *  Resolução (padrão do sistema; ajustada automaticamente em
 *  video_init() caso o hardware real seja diferente)
 * ============================================================ */
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240

/* ============================================================
 *  Cores em RGB 24-bit (0xRRGGBB) — convertidas automaticamente
 *  para o formato real do hardware (16-bit ou 8-bit) na hora de
 *  desenhar, via resample interno.
 * ============================================================ */
#define COLOR_BLACK      0x000000
#define COLOR_WHITE      0xFFFFFF
#define COLOR_RED        0xFF0000
#define COLOR_GREEN      0x00FF00
#define COLOR_BLUE       0x0000FF
#define COLOR_YELLOW     0xFFFF00
#define COLOR_CYAN       0x00FFFF
#define COLOR_MAGENTA    0xFF00FF
#define COLOR_ORANGE     0xFFA500
#define COLOR_DARK_GRAY  0x404040
#define COLOR_LIGHT_GRAY 0xC0C0C0
#define COLOR_DARK_BLUE  0x000080

/* Cores dos tetrominós (I, O, T, S, Z, J, L) */
#define COLOR_PIECE_I    0x00FFFF   /* Ciano   */
#define COLOR_PIECE_O    0xFFFF00   /* Amarelo */
#define COLOR_PIECE_T    0xFF00FF   /* Magenta */
#define COLOR_PIECE_S    0x00FF00   /* Verde   */
#define COLOR_PIECE_Z    0xFF0000   /* Vermelho*/
#define COLOR_PIECE_J    0x0000FF   /* Azul    */
#define COLOR_PIECE_L    0xFFA500   /* Laranja */
#define COLOR_GHOST      0x404040   /* Cinza escuro (ghost) */

/* ============================================================
 *  Funções do driver de vídeo
 * ============================================================ */

/* Detecta resolução/profundidade de cor reais e prepara o driver.
 * Chamar uma vez no início do programa. */
void video_init(void);

/* Limpa a tela inteira com uma cor (RGB 24-bit) */
void video_clear(int color);

/* Desenha um retângulo preenchido. (x, y) = canto superior-esquerdo,
 * em pixels lógicos de 320×240 — o driver ajusta automaticamente
 * para a resolução/profundidade de cor reais do hardware. */
void video_draw_rect(int x, int y, int w, int h, int color);

/* Desenha apenas a borda (1px) de um retângulo */
void video_draw_rect_outline(int x, int y, int w, int h, int color);

/* Escreve texto usando o buffer de caracteres do hardware.
 * IMPORTANTE: (col, row) são coordenadas de CÉLULA DE CARACTERE
 * (grade de ~40 colunas × ~30 linhas), NÃO pixels. */
void video_text(int col, int row, const char *text);

/* Apaga `width` caracteres a partir de (col, row) */
void video_clear_text(int col, int row, int width);

#endif /* VIDEO_H */
