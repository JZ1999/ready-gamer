/*
 * Headless Ready Gamer e2e runner (Peanut-GB).
 *   rgrun ROM NOI OUTDIR MAX_FRAMES [LOG_EVERY] [SHOT_EVERY] [sym:size ...]
 * - Runs the ROM with no input (the in-ROM autopilot presses the buttons).
 * - Stops at EMU_BREAKPOINT (ld b,b) or MAX_FRAMES.
 * - Every LOG_EVERY frames appends the watched symbols to OUTDIR/telemetry.csv.
 * - Every SHOT_EVERY frames dumps a 160x144 PGM to OUTDIR/frames/NNNNNN.pgm.
 * - Writes OUTDIR/final.pgm and OUTDIR/result.txt (frames, breakpoint, verdict).
 */
#define ENABLE_LCD 1
#define ENABLE_SOUND 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "peanut_gb_e2e.h"

static uint8_t *rom;
static size_t rom_size;
static uint8_t cart_ram[0x20000];
static uint8_t fb[144][160];

static uint8_t rom_read(struct gb_s *gb, const uint_fast32_t a) { (void)gb; return a < rom_size ? rom[a] : 0xFF; }
static uint8_t ram_read(struct gb_s *gb, const uint_fast32_t a) { (void)gb; return cart_ram[a & 0x1FFFF]; }
static void ram_write(struct gb_s *gb, const uint_fast32_t a, const uint8_t v) { (void)gb; cart_ram[a & 0x1FFFF] = v; }
static void on_error(struct gb_s *gb, const enum gb_error_e e, const uint16_t addr) {
	(void)gb; fprintf(stderr, "emu error %d at %04X\n", e, addr);
}
static void draw_line(struct gb_s *gb, const uint8_t *px, const uint_fast8_t line) {
	(void)gb; for (int x = 0; x < 160; x++) fb[line][x] = px[x] & 3;
}

typedef struct { char name[96]; uint16_t addr; int size; } Sym;
static Sym syms[64]; static int nsyms;

static int noi_lookup(const char *noi, const char *name, uint16_t *out) {
	FILE *f = fopen(noi, "r"); char line[512], key[300]; unsigned v;
	if (!f) return 0;
	snprintf(key, sizeof key, "DEF %s ", name);
	while (fgets(line, sizeof line, f)) {
		if (!strncmp(line, key, strlen(key)) && sscanf(line + strlen(key), "0x%x", &v) == 1) {
			fclose(f); *out = (uint16_t)v; return 1;
		}
	}
	fclose(f); return 0;
}

static uint8_t rd8(struct gb_s *gb, uint16_t a) {
	if (a >= 0xC000 && a < 0xE000) return gb->wram[a - 0xC000];
	if (a >= 0xFF00) return gb->hram_io[a - 0xFF00];
	if (a >= 0xFE00 && a < 0xFEA0) return gb->oam[a - 0xFE00];
	return 0;
}
static unsigned rdsym(struct gb_s *gb, Sym *s) {
	return s->size == 2 ? (unsigned)(rd8(gb, s->addr) | (rd8(gb, s->addr + 1) << 8))
	                    : (s->size == 4 ? (unsigned)(rd8(gb, s->addr) | (rd8(gb, s->addr+1) << 8) |
	                                      (rd8(gb, s->addr+2) << 16) | ((unsigned)rd8(gb, s->addr+3) << 24))
	                                    : rd8(gb, s->addr));
}

static void write_pgm(const char *path) {
	static const uint8_t shade[4] = { 224, 136, 52, 8 };
	FILE *f = fopen(path, "wb"); if (!f) return;
	fprintf(f, "P5 160 144 255\n");
	for (int y = 0; y < 144; y++) for (int x = 0; x < 160; x++) fputc(shade[fb[y][x]], f);
	fclose(f);
}

int main(int argc, char **argv) {
	if (argc < 5) { fprintf(stderr, "usage: rgrun ROM NOI OUTDIR MAX_FRAMES [LOG_EVERY] [SHOT_EVERY] [sym:size...]\n"); return 2; }
	const char *romp = argv[1], *noi = argv[2], *out = argv[3];
	long maxf = atol(argv[4]); int log_every = argc > 5 ? atoi(argv[5]) : 60; int shot_every = argc > 6 ? atoi(argv[6]) : 0;
	char path[1024];
	FILE *rf = fopen(romp, "rb"); if (!rf) { perror(romp); return 2; }
	fseek(rf, 0, SEEK_END); rom_size = ftell(rf); fseek(rf, 0, SEEK_SET);
	rom = malloc(rom_size); fread(rom, 1, rom_size, rf); fclose(rf);
	mkdir(out, 0755); snprintf(path, sizeof path, "%s/frames", out); if (shot_every) mkdir(path, 0755);

	for (int i = 7; i < argc && nsyms < 64; i++) {
		char nm[96]; int sz = 1; char *c = strchr(argv[i], ':');
		if (c) { snprintf(nm, sizeof nm, "%.*s", (int)(c - argv[i]), argv[i]); sz = atoi(c + 1); } else snprintf(nm, sizeof nm, "%s", argv[i]);
		/* "_sym+N" reads at an offset (array elements) */
		int off = 0; char base[96]; char *pl = strchr(nm, '+');
		snprintf(base, sizeof base, "%s", nm); if (pl) { base[pl - nm] = 0; off = atoi(pl + 1); }
		if (noi_lookup(noi, base, &syms[nsyms].addr)) { syms[nsyms].addr += off; snprintf(syms[nsyms].name, sizeof syms[nsyms].name, "%s", nm); syms[nsyms].size = sz; nsyms++; }
		else fprintf(stderr, "warn: symbol %s not found\n", nm);
	}
	static struct gb_s gb;
	if (gb_init(&gb, rom_read, ram_read, ram_write, on_error, NULL) != GB_INIT_NO_ERROR) { fprintf(stderr, "gb_init failed\n"); return 2; }
	gb_init_lcd(&gb, draw_line);

	snprintf(path, sizeof path, "%s/telemetry.csv", out);
	FILE *tl = fopen(path, "w"); fprintf(tl, "frame");
	for (int i = 0; i < nsyms; i++) fprintf(tl, ",%s", syms[i].name + (syms[i].name[0] == '_'));
	fprintf(tl, "\n");

	long prof_from = getenv("RG_PROF") ? atol(getenv("RG_PROF")) : -1;
	/* RG_INCL="name1 name2 ..." (rom.noi names): inclusive instruction counts. */
	if (getenv("RG_INCL")) {
		char buf[1024]; snprintf(buf, sizeof buf, "%s", getenv("RG_INCL"));
		for (char *t = strtok(buf, " "); t && gb_incl_n < GB_INCL_MAX; t = strtok(0, " ")) {
			uint16_t a16; FILE *nf = fopen(noi, "r"); char line[512], key[300]; unsigned v; int ok = 0;
			while (nf && fgets(line, sizeof line, nf)) if (sscanf(line, "DEF %299s 0x%x", key, &v) == 2 && !strcmp(key, t)) { gb_incl_addr[gb_incl_n] = v; ok = 1; break; }
			if (nf) fclose(nf); (void)a16;
			if (ok) { gb_incl_depth_sp[gb_incl_n] = -1; gb_incl_n++; } else fprintf(stderr, "RG_INCL: no symbol %s\n", t);
		}
	}
	long f = 0; int bp = 0;
	for (; f < maxf; f++) {
		if (prof_from >= 0 && f == prof_from) gb_prof = calloc((size_t)64 << 16, sizeof(unsigned));
		gb_run_frame(&gb);
		if (log_every && f % log_every == 0) {
			fprintf(tl, "%ld", f); for (int i = 0; i < nsyms; i++) fprintf(tl, ",%u", rdsym(&gb, &syms[i])); fprintf(tl, "\n");
		}
		if (shot_every && f % shot_every == 0) { snprintf(path, sizeof path, "%s/frames/%07ld.pgm", out, f); write_pgm(path); }
		if (gb_e2e_breakpoint_hit) { bp = 1; break; }
	}
	fprintf(tl, "%ld", f); for (int i = 0; i < nsyms; i++) fprintf(tl, ",%u", rdsym(&gb, &syms[i])); fprintf(tl, "\n");
	fclose(tl);
	snprintf(path, sizeof path, "%s/final.pgm", out); write_pgm(path);
	if (gb_incl_n) {
		snprintf(path, sizeof path, "%s/incl.txt", out); FILE *pf = fopen(path, "w");
		for (int i = 0; i < gb_incl_n; i++) fprintf(pf, "%x calls=%lu instr=%llu per_call=%.0f\n", gb_incl_addr[i], gb_incl_calls[i], gb_incl_instr[i], gb_incl_calls[i] ? (double)gb_incl_instr[i] / gb_incl_calls[i] : 0.0);
		fclose(pf);
	}
	if (gb_prof) { /* raw dump; tools/e2e/linux/prof.py maps it to functions */
		snprintf(path, sizeof path, "%s/prof.txt", out); FILE *pf = fopen(path, "w");
		for (unsigned a = 0; a < (64u << 16); a++) if (gb_prof[a]) fprintf(pf, "%x %u\n", a, gb_prof[a]);
		fclose(pf);
	}

	/* Verdict from the painted frame: all lightest = PASS, all darkest = FAIL. */
	int light = 1, dark = 1;
	for (int y = 0; y < 144; y++) for (int x = 0; x < 160; x++) { if (fb[y][x] != 0) light = 0; if (fb[y][x] != 3) dark = 0; }
	const char *verdict = bp ? (light ? "PASS" : dark ? "FAIL" : "NONE") : "TIMEOUT";
	snprintf(path, sizeof path, "%s/result.txt", out);
	FILE *r = fopen(path, "w"); fprintf(r, "verdict=%s\nframes=%ld\nbreakpoint=%d\n", verdict, f, bp); fclose(r);
	printf("verdict=%s frames=%ld (%.1f emulated s)\n", verdict, f, f / 59.73);
	return strcmp(verdict, "PASS") == 0 ? 0 : 1;
}
