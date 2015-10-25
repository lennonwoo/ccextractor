
#include "ccx_decoders_isdb.h"
#include "lib_ccx.h"
#include "utility.h"
#include "limits.h"

#define DEBUG
#define COMMAND_DEBUG

#ifdef DEBUG
	#define isdb_log( format, ... ) mprint(format, ##__VA_ARGS__ )
	#ifdef COMMAND_DEBUG
		#define isdb_command_log( format, ... ) mprint(format, ##__VA_ARGS__ )
	#else
		#define isdb_command_log( format, ... ) mprint(format, ##__VA_ARGS__ )
	#endif
#else
	#define isdb_log( ...) ((void)0)
#endif

enum writing_format
{
	WF_HORIZONTAL_STD_DENSITY = 0,
	WF_VERTICAL_STD_DENSITY = 1,
	WF_HORIZONTAL_HIGH_DENSITY = 2,
	WF_VERTICAL_HIGH_DENSITY = 3,
	WF_HORIZONTAL_WESTERN_LANG = 4,
	WF_HORIZONTAL_1920x1080 = 5,
	WF_VERTICAL_1920x1080 = 6,
	WF_HORIZONTAL_960x540 = 7,
	WF_VERTICAL_960x540 = 8,
	WF_HORIZONTAL_720x480 = 9,
	WF_VERTICAL_720x480 = 10,
	WF_HORIZONTAL_1280x720 = 11,
	WF_VERTICAL_1280x720 = 12,
	WF_HORIZONTAL_CUSTOM = 100,
	WF_NONE,
};

enum fontSize
{
	SMALL_FONT_SIZE,
	MIDDLE_FONT_SIZE,
	STANDARD_FONT_SIZE,
};

enum csi_command
{
	/* GSM Character Deformation*/
	CSI_CMD_GSM = 0x42,
	/* Set Writing Format */
	CSI_CMD_SWF = 0x53,
	/* Composite Character Composition */
	CSI_CMD_CCC = 0x54,
	/* Set Display Format */
	CSI_CMD_SDF = 0x56,
	/* Character composition dot designation */
	CSI_CMD_SSM = 0x57,
	/* Set HOrizontal spacing */
	CSI_CMD_SHS = 0x58,
	/* Set Vertical Spacing */
	CSI_CMD_SVS = 0x59,
	/* Partially Line Down */
	CSI_CMD_PLD = 0x5B,
	/* Partially Line UP */
	CSI_CMD_PLU = 0x5C,
	/* Colouring block */
	CSI_CMD_GAA = 0x5D,
	/* Raster Colour Designation */
	CSI_CMD_SRC = 0x5E,
	/* Set Display Position */
	CSI_CMD_SDP = 0x5F,
	/* Active Coordinate Position Set */
	CSI_CMD_ACPS = 0x61,
	/* Switch control */
	CSI_CMD_TCC = 0x62,
	/* Ornament Control */
	CSI_CMD_ORN = 0x63,
	/* Font */
	CSI_CMD_MDF = 0x64,
	/* Character Font Set */
	CSI_CMD_CFS = 0x65,
	/* External Character Set */
	CSI_CMD_XCS = 0x66,
	/* Built-in sound replay */
	CSI_CMD_PRA = 0x68,
	/* Alternative Character Set */
	CSI_CMD_ACS = 0x69,
	/* Raster Color Command */
	CSI_CMD_RCS = 0x6E,
	/* Skip Character Set */
	CSI_CMD_SCS = 0x6F,
};

enum isdb_CC_composition
{
	ISDB_CC_NONE = 0,
	ISDB_CC_AND  = 2,
	ISDB_CC_OR   = 3,
	ISDB_CC_XOR  = 4,
};

enum color
{
	CCX_ISDB_BLACK,
	FI_RED,
	FI_GREEN,
	FI_YELLOW,
	FI_BLUE,
	FI_MAGENTA,
	FI_CYAN,
	FI_WHITE,
	CCX_ISDB_TRANSPARENT,
	HI_RED,
	HI_GREEN,
	HI_YELLOW,
	HI_BLUE,
	HI_MAGENTA,
	HI_CYAN,
	HI_WHITE,
};

enum isdb_tmd
{
	ISDB_TMD_FREE = 0,
	ISDB_TMD_REAL_TIME = 0x1,
	ISDB_TMD_OFFSET_TIME = 0x2,
};

#define IS_HORIZONTAL_LAYOUT(format) \
	( (format) == WF_HORIZONTAL_STD_DENSITY\
	||(format) == WF_HORIZONTAL_HIGH_DENSITY\
	||(format) == WF_HORIZONTAL_WESTERN_LANG\
	||(format) == WF_HORIZONTAL_1920x1080\
	||(format) == WF_HORIZONTAL_960x540\
	||(format) == WF_HORIZONTAL_720x480\
	||(format) == WF_HORIZONTAL_1280x720\
	||(format) == WF_HORIZONTAL_CUSTOM )

#define LAYOUT_GET_WIDTH(format) \
	(((format) == ISDBSUB_FMT_960H || (format) == ISDBSUB_FMT_960V) ? 960 : 720)
#define LAYOUT_GET_HEIGHT(format) \
	(((format) == ISDBSUB_FMT_960H || (format) == ISDBSUB_FMT_960V) ? 540 : 480)

#define RGBA(r,g,b,a) (((unsigned)(255 - (a)) << 24) | ((b) << 16) | ((g) << 8) | (r))
typedef uint32_t rgba;
static rgba Default_clut[128] =
{
	//0-7
	RGBA(0,0,0,255), RGBA(255,0,0,255), RGBA(0,255,0,255), RGBA(255,255,0,255),
	RGBA(0,0,255,255), RGBA(255,0,255,255), RGBA(0,255,255,255), RGBA(255,255,255,255),
	//8-15
	RGBA(0,0,0,0), RGBA(170,0,0,255), RGBA(0,170,0,255), RGBA(170,170,0,255),
	RGBA(0,0,170,255), RGBA(170,0,170,255), RGBA(0,170,170,255), RGBA(170,170,170,255),
	//16-23
	RGBA(0,0,85,255), RGBA(0,85,0,255), RGBA(0,85,85,255), RGBA(0,85,170,255),
	RGBA(0,85,255,255), RGBA(0,170,85,255), RGBA(0,170,255,255), RGBA(0,255,85,255),
	//24-31
	RGBA(0,255,170,255), RGBA(85,0,0,255), RGBA(85,0,85,255), RGBA(85,0,170,255),
	RGBA(85,0,255,255), RGBA(85,85,0,255), RGBA(85,85,85,255), RGBA(85,85,170,255),
	//32-39
	RGBA(85,85,255,255), RGBA(85,170,0,255), RGBA(85,170,85,255), RGBA(85,170,170,255),
	RGBA(85,170,255,255), RGBA(85,255,0,255), RGBA(85,255,85,255), RGBA(85,255,170,255),
	//40-47
	RGBA(85,255,255,255), RGBA(170,0,85,255), RGBA(170,0,255,255), RGBA(170,85,0,255),
	RGBA(170,85,85,255), RGBA(170,85,170,255), RGBA(170,85,255,255), RGBA(170,170,85,255),
	//48-55
	RGBA(170,170,255,255), RGBA(170,255,0,255), RGBA(170,255,85,255), RGBA(170,255,170,255),
	RGBA(170,255,255,255), RGBA(255,0,85,255), RGBA(255,0,170,255), RGBA(255,85,0,255),
	//56-63
	RGBA(255,85,85,255), RGBA(255,85,170,255), RGBA(255,85,255,255), RGBA(255,170,0,255),
	RGBA(255,170,85,255), RGBA(255,170,170,255), RGBA(255,170,255,255), RGBA(255,255,85,255),
	//64
	RGBA(255,255,170,255),
	// 65-127 are caliculated later.
};
struct b24str_state
{
	int gl;/* index of the group invoked to GL */
	int gr;/* index of the group invoked to GR */
	int ss;/* flag if in SS2 or SS3. 2:SS2, 3:SS3 */

	struct group
	{
		unsigned char mb;/* how many bytes one character consists of. */
		// code for character sets
		#define CODE_ASCII ('\x40')
		#define CODE_ASCII2 ('\x4A')
		#define CODE_JISX0208 ('\x42')
		#define CODE_JISX0213_1 ('\x51')
		#define CODE_JISX0213_2 ('\x50')
		#define CODE_JISX0201_KATA ('\x49')
		#define CODE_MOSAIC_C ('\x34')
		#define CODE_MOSAIC_D ('\x35')
		#define CODE_EXT ('\x3B')
		#define CODE_X_HIRA ('\x30')
		#define CODE_X_HIRA_P ('\x37')
		#define CODE_X_KATA ('\x31')
		#define CODE_X_KATA_P ('\x38')
		#define CODE_X_DRCS_MB ('\x40')
		#define CODE_X_DRCS_MIN ('\x41')
		#define CODE_X_DRCS_MAX ('\x4F')
		#define CODE_X_MACRO ('\x70')
		unsigned char code;/* character set that this group designates */
	} g[4];
};

typedef struct
{
	enum writing_format format;
	int is_profile_c;// profile C: "1seg". see ARIB TR-B14 3-4

	// clipping area.
	struct disp_area {
		int x, y;
		int w, h;
	} display_area;

	// for tracking pen position
	int font_size; // valid values: {16, 20, 24, 30, 36} (TR-B14/B15)
	struct fscale { // in [percent]
		int fscx, fscy;
	} font_scale; // 1/2x1/2, 1/2*1, 1*1, 1*2, 2*1, 2*2
	struct spacing {
		int col, row;
	} cell_spacing;

	// internal use for tracking pen position/line break.
	// Although texts are laid out by libass,
	// we need to track pen position by ourselves
	// in order to calculate line breaking positions and charcter/line spacing.
	int prev_char_sep;
	int prev_line_desc;
	int prev_line_bottom; // offset from display_area.y, not from top-margin.
	int line_desc;
	int linesep_upper;
	int line_height;
	int line_width; // pen position x
	int prev_break_idx; // ctx->text.buf[prev_break_idx] holds the previous "\N"
	int shift_baseline; // special case where baseline should be shifted down ?

	int block_offset_h;// text[0].hspacing / 2
	int block_offset_v;// line[0].lspacing_upper

	int repeat_count; // -1: none, 0: until EOL, 1...i: repeat the next char i times
	int in_combining; // bool
	enum isdb_CC_composition ccc;
	int acps[2];
	struct scroll_param 
	{
		enum {SCROLL_DIR_NONE, SCROLL_DIR_COLUMN, SCROLL_DIR_ROW} direction;
		int rollout;// bool
		int speed;// in pixel/sec
	} scroll;

}ISDBSubLayout;

typedef struct {
	int auto_display; // bool. forced to be displayed w/o user interaction
	int rollup_mode;  // bool

	uint8_t need_init; // bool
	uint8_t clut_high_idx; // color = default_clut[high_idx << 8 | low_idx]

	uint32_t fg_color;
	uint32_t bg_color;
	/**
	 * Colour between foreground and background in gradation font is defined that
	 * colour near to foreground colour is half foreground colour and colour near to
	 * background colour is half background colour.
	 */
	//Half forground color
	uint32_t hfg_color;
	//Half background color
	uint32_t hbg_color;
	uint32_t mat_color;

	ISDBSubLayout layout_state;
	struct b24str_state text_state;
} ISDBSubState;

typedef struct
{
	char *char_buf;
	int char_buf_index;
	int len;
	int nb_char;
	int nb_line;
	uint64_t timestamp;
	uint64_t prev_timestamp;
	struct {
		int raster_color;
		int clut_high_idx;
	}state;
	ISDBSubState current_state; //modified default_state[lang_tag]
	struct {
		char *buf;
		size_t len;
		size_t used;
		size_t txt_tail; // tail of the text, excluding trailing control sequences.
	} text;
	enum isdb_tmd tmd;
	int nb_lang;
	struct
	{
		int hour;
		int min;
		int sec;
		int milli;
	}offset_time;
	uint8_t dmf;
	uint8_t dc;

}ISDBSubContext;


void delete_isdb_caption(void *isdb_ctx)
{
	ISDBSubContext *ctx = isdb_ctx;
	free(ctx->char_buf);
	free(ctx);
}

void *init_isdb_caption(void)
{
	ISDBSubContext *ctx;

	ctx = malloc(sizeof(ISDBSubContext));
	if(!ctx)
		return NULL;

	ctx->char_buf = malloc(1024);
	if(!ctx->char_buf)
	{
		free(ctx);
		return NULL;
	}
	ctx->len = 1024;
	ctx->char_buf_index = 0;
	ctx->prev_timestamp = UINT_MAX;
	return ctx;
}

static void advance(ISDBSubContext *ctx)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;
	int cscale;
	int h;
	int asc, desc;
	int csp;

	if (IS_HORIZONTAL_LAYOUT(ls->format))
	{
		cscale = ls->font_scale.fscx;
		h = ls->font_size * ls->font_scale.fscy / 100;
		if (ls->font_scale.fscy == 200)
		{
			desc = ls->cell_spacing.row / 2;
			asc = ls->cell_spacing.row * 2 - desc + h;
		}
		else
		{
			desc = ls->cell_spacing.row * ls->font_scale.fscy / 200;
			asc = ls->cell_spacing.row * ls->font_scale.fscy / 100 - desc + h;
		}
		if (asc > ls->line_height + ls->linesep_upper)
		{
			if (h > ls->line_height)
				ls->line_height = h;
			ls->linesep_upper = asc - ls->line_height;
		}
		else if (h > ls->line_height)
		{
			ls->linesep_upper = ls->line_height + ls->linesep_upper - h;
			ls->line_height = h;
		}

		if (ls->prev_line_bottom == 0 && ls->linesep_upper > ls->block_offset_v)
			ls->block_offset_v = ls->linesep_upper;
		if (ls->font_scale.fscy != 50)
			ls->shift_baseline = 0;
	}
	else
	{
		int lsp;
		cscale = ls->font_scale.fscy;
		h = ls->font_size * ls->font_scale.fscx / 100;
		lsp = ls->cell_spacing.row * ls->font_scale.fscx / 100;
		desc = h / 2 + lsp / 2;
		asc = h - h / 2 + lsp - lsp / 2;
		if (asc > ls->line_height + ls->linesep_upper)
		{
			if (h - h / 2 > ls->line_height)
				ls->line_height = h - h / 2;
			ls->linesep_upper = asc - ls->line_height;
		}
		else if (h - h / 2 > ls->line_height)
		{
			ls->linesep_upper = ls->line_height + ls->linesep_upper - h + h / 2;
		}
		else if (h - h / 2 > ls->line_height)
		{
			ls->linesep_upper = ls->line_height + ls->linesep_upper - h + h / 2;
			ls->line_height = h - h / 2;
		}

		if (ls->prev_line_bottom == 0 && ls->linesep_upper > ls->block_offset_h)
			ls->block_offset_h = ls->linesep_upper;
		ls->shift_baseline = 0;
	}
	if (desc > ls->line_desc)
		ls->line_desc = desc;

	csp = ls->cell_spacing.col * cscale / 100;
	ls->line_width += ls->font_size * cscale / 100 + csp;
	ls->prev_char_sep = csp;
}

static void reserve_buf(ISDBSubContext *ctx, size_t len)
{
	size_t blen;

	if (ctx->text.len >= ctx->text.used + len)
		return;

	blen = ((ctx->text.used + len + 127) >> 7) << 7;
	ctx->text.buf = realloc(ctx->text.buf, blen);
	if (!ctx->text.buf)
	{
		isdb_log("out of memory for ctx->text.\n");
		return;
	}
	ctx->text.len = blen;
	isdb_log ("expanded ctx->text(%lu)\n", blen);
}

static int append_char(ISDBSubContext *ctx, const char ch)
{
	if (!ctx->text.used && ( ch == '\n' || ch == '\r') )
		return 1;
	reserve_buf(ctx, 2); // +1 for terminating '\0'
	ctx->text.buf[ctx->text.used] = ch;
	ctx->text.used ++;
	ctx->text.buf[ctx->text.used] = '\0';

	return 1;
}
static void insert_str(ISDBSubContext *ctx, const char *txt, int begin)
{
	int end = ctx->text.used;
	size_t len = strlen(txt);

	if (len == 0 || len > 128)
		return;

	reserve_buf(ctx, len + 1); // +1 for terminating '\0'
	memmove(ctx->text.buf + begin + len, ctx->text.buf + begin, end - begin);
	memcpy(ctx->text.buf + begin, txt, len);
	ctx->text.txt_tail += len;
	ctx->text.used += len;
	ctx->text.buf[ctx->text.used] = '\0';
}

static void advance_by_pixels(ISDBSubContext *ctx, int csp)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;
	int cscale;
	int csep_orig;

	if (IS_HORIZONTAL_LAYOUT(ls->format))
		cscale = ls->font_scale.fscx;
	else
		cscale = ls->font_scale.fscy;
	csep_orig = ls->cell_spacing.col * cscale / 100;

	ls->line_width += csp;
	ls->prev_char_sep = csep_orig;
	isdb_log("advanced %d pixel using fsp.\n", csp);
}

static void fixup_linesep(ISDBSubContext *ctx)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;
	//char tmp[16];
	//int lsp;

	if (ls->prev_break_idx <= 0)
		return;
	// adjust baseline if all chars in the line are 50% tall of one font size.
	if (ls->shift_baseline && IS_HORIZONTAL_LAYOUT(ls->format))
	{
		int delta = ls->cell_spacing.row / 4;
		ls->linesep_upper += delta;
		ls->line_desc -= delta;
		isdb_log("baseline shifted down %dpx.\n", delta);
	}

}

static void do_line_break(ISDBSubContext *ctx)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;
	int csp;

	if (IS_HORIZONTAL_LAYOUT(ls->format))
		csp = ls->cell_spacing.col * ls->font_scale.fscx / 100;
	else
		csp = ls->cell_spacing.col * ls->font_scale.fscy / 100;

	if (ls->line_width == 0)
	{
		if (ls->prev_line_bottom == 0)
		{
			if (IS_HORIZONTAL_LAYOUT(ls->format))
				ls->block_offset_h = csp / 2;
			else
				ls->block_offset_v = csp / 2;
		}

		// avoid empty lines by prepending a space,
		// as ASS halves line-spacing of empty lines.
		//append_str(ctx, "\xe3\x80\x80");
		advance(ctx);
	}

	append_char(ctx, '\r');
	append_char(ctx, '\n');
	fixup_linesep(ctx);

	ls->prev_break_idx = ctx->text.used;

	ls->prev_line_desc = ls->line_desc;
	ls->prev_line_bottom += ls->linesep_upper + ls->line_height + ls->line_desc;
	ls->prev_char_sep = csp;
	ls->line_height = 0;
	ls->line_width = 0;
	ls->line_desc = 0;
	ls->linesep_upper = 0;
	ls->shift_baseline = 0;
}

static void set_writing_format(ISDBSubContext *ctx, uint8_t *arg)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;

	/* One param means its initialization */
	if( *(arg+1) == 0x20)
	{
		ls->format = (arg[0] & 0x0F);
		return;
	}

	/* P1 I1 p2 I2 P31 ~ P3i I3 P41 ~ P4j I4 F */
	if ( *(arg + 1) == 0x3B)
	{
		ls->format = WF_HORIZONTAL_CUSTOM;
		arg += 2;
	}
	if ( *(arg + 1) == 0x3B)
	{
		switch (*arg & 0x0f) {
		case 0:
			//ctx->font_size = SMALL_FONT_SIZE;
			break;
		case 1:
			//ctx->font_size = MIDDLE_FONT_SIZE;
			break;
		case 2:
			//ctx->font_size = STANDARD_FONT_SIZE;
		default:
			break;
		}
		arg += 2;
	}
	/* P3 */
	isdb_log("character numbers in one line in decimal:");
	while (*arg != 0x3b && *arg != 0x20)
	{
		ctx->nb_char = *arg;
		printf(" %x",*arg & 0x0f);
		arg++;
	}
	if (*arg == 0x20)
		return;
	arg++;
	isdb_log("line numbers in decimal: ");
	while (*arg != 0x20)
	{
		ctx->nb_line = *arg;
		printf(" %x",*arg & 0x0f);
		arg++;
	}

	return;
}

/** move pen position to (col, row) relative to display area's top left.
 *  Note 1: In vertical layout, coordinates are rotated 90 deg.
 *          on the display area's top right.
 *  Note 2: the cell includes line/char spacings in both sides.
 *  NOte 3: Lmt) == `gic taken from Mplayer
 */
static void move_penpos(ISDBSubContext *ctx, int col, int row)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;
	int csp_l, col_ofs;
	int cur_bottom;
	int cell_height;
	int cell_desc;

	if (IS_HORIZONTAL_LAYOUT(ls->format))
	{
		// convert pen pos. to upper left of the cell.
		cell_height = (ls->font_size + ls->cell_spacing.row)
			* ls->font_scale.fscy / 100;

		if (ls->font_scale.fscy == 200)
			cell_desc = ls->cell_spacing.row / 2;
		else
			cell_desc = ls->cell_spacing.row * ls->font_scale.fscy / 200;
		row -= cell_height;

		csp_l = ls->cell_spacing.col * ls->font_scale.fscx / 200;
		if (ls->line_width == 0 && ls->prev_line_bottom == 0)
			ls->block_offset_h = csp_l;
		col_ofs = ls->block_offset_h;
	}
	else
	{
		cell_height = (ls->font_size + ls->cell_spacing.row)
			* ls->font_scale.fscx / 100;
		cell_desc = cell_height / 2;
		row -= cell_height - cell_desc;

		csp_l = ls->cell_spacing.col * ls->font_scale.fscy / 200;
		if (ls->line_width == 0 && ls->prev_line_bottom == 0)
			ls->block_offset_v = csp_l;
		col_ofs = ls->block_offset_v;
	}

	cur_bottom = ls->prev_line_bottom +
		ls->linesep_upper + ls->line_height + ls->line_desc;
	// allow adjusting +- cell_height/2 at maximum
	//     to align to the current line bottom.
	isdb_log("move pen pos from %d to %d\n", cur_bottom, row + cell_height / 2);
	//if (row + cell_height / 2 >= cur_bottom)
	if (row + cell_height / 2 > cur_bottom)
	{
		do_line_break(ctx); // ls->prev_line_bottom == cur_bottom
		ls->linesep_upper = row + cell_height - cell_desc - ls->prev_line_bottom;
		ls->line_height = 0;

		advance_by_pixels(ctx, col + csp_l - col_ofs);
	}
	else if (row + cell_height * 3 / 2 > cur_bottom &&
			col + csp_l > col_ofs + ls->line_width)
	{
		// append to the current line...
		advance_by_pixels(ctx, col + csp_l - (col_ofs + ls->line_width));
	}
	else
	{
		isdb_log ("ENOSUPP: backward move not supported.\n");
	}
}


static void set_position(ISDBSubContext *ctx, unsigned int p1, unsigned int p2)
{
	ISDBSubLayout *ls = &ctx->current_state.layout_state;
	int cw, ch;
	int col, row;


	if (IS_HORIZONTAL_LAYOUT(ls->format))
	{
		cw = (ls->font_size + ls->cell_spacing.col) * ls->font_scale.fscx / 100;
		ch = (ls->font_size + ls->cell_spacing.row) * ls->font_scale.fscy / 100;
		// pen position is at bottom left
		col = p2 * cw;
		row = p1 * ch + ch;
	}
	else
	{
		cw = (ls->font_size + ls->cell_spacing.col) * ls->font_scale.fscy / 100;
		ch = (ls->font_size + ls->cell_spacing.row) * ls->font_scale.fscx / 100;
		// pen position is at upper center,
		// but in -90deg rotated coordinates, it is at middle left.
		col = p2 * cw;
		row = p1 * ch + ch / 2;
	}
	move_penpos(ctx, col, row);
}

static int get_csi_params(const uint8_t *q, unsigned int *p1, unsigned int *p2)
{
	const uint8_t *q_pivot = q;
	if (!p1)
		return -1;

	*p1 = 0;
	for (; *q >= 0x30 && *q <= 0x39; q++)
	{
		*p1 *= 10;
		*p1 += *q - 0x30;
	}
	if (*q != 0x20 && *q != 0x3B)
		return -1;
	q++;
	if (!p2)
		return q - q_pivot;
	*p2 = 0;
	for (; *q >= 0x30 && *q <= 0x39; q++)
	{
		*p2 *= 10;
		*p2 += *q - 0x30;
	}
	q++;
	return q - q_pivot;
}

static int parse_csi(ISDBSubContext *ctx, const uint8_t *buf, int len)
{
	uint8_t arg[10] = {0};
	int i = 0;
	int ret = 0;
	unsigned int p1,p2;
	const uint8_t *buf_pivot = buf;
	ISDBSubState *state = &ctx->current_state;
	ISDBSubLayout *ls = &state->layout_state;

	//Copy buf in arg
	for(i = 0; *buf != 0x20; i++)
	{
		if (i >= (sizeof(arg))+ 1)
		{
			isdb_log("UnExpected CSI %d >= %d", sizeof(arg) + 1, i);
			break;
		}
		arg[i] = *buf;
		buf++;
	}
	/* ignore terminating 0x20 character */
	arg[i] = *buf++;

	switch(*buf) {
	/* Set Writing Format */
	case CSI_CMD_SWF:
		isdb_command_log("Command:CSI: SWF\n");
		set_writing_format(ctx, arg);
		break;
	/* Composite Character Composition */
	case CSI_CMD_CCC:
		isdb_command_log("Command:CSI: CCC\n");
		ret = get_csi_params(arg, &p1, NULL);
		if (ret > 0)
		{
			ls->ccc = p1;
		}
		break;
	/* Set Display Format */
	case CSI_CMD_SDF:
		ret = get_csi_params(arg, &p1, &p2);
		if (ret > 0)
		{
			ls->display_area.w = p1;
			ls->display_area.h = p2;
		}
		isdb_command_log("Command:CSI: SDF (w:%d, h:%d)\n", p1, p2);
		break;
	/* Character composition dot designation */
	case CSI_CMD_SSM:
		ret = get_csi_params(arg, &p1, &p2);
		if (ret > 0)
			ls->font_size = p1;
		isdb_command_log("Command:CSI: SSM (x:%d y:%d)\n", p1, p2);
		break;
	/* Set Display Position */
	case CSI_CMD_SDP:
		ret = get_csi_params(arg, &p1, &p2);
		if (ret > 0)
		{
			ls->display_area.x = p1;
			ls->display_area.y = p2;
		}
		isdb_command_log("Command:CSI: SDP (x:%d, y:%d)\n", p1, p2);
		break;
	/* Raster Colour command */
	case CSI_CMD_RCS:
		ret = get_csi_params(arg, &p1, NULL);
		if (ret > 0)
			ctx->state.raster_color = Default_clut[ctx->state.clut_high_idx << 4 | p1];
		isdb_command_log("Command:CSI: RCS (%d)\n", p1);
		break;
	/* Set Horizontal Spacing */
	case CSI_CMD_SHS:
		ret = get_csi_params(arg, &p1, NULL);
		if (ret > 0)
			ls->cell_spacing.col = p1;
		isdb_command_log("Command:CSI: SHS (%d)\n", p1);
		break;
	/* Set Vertical Spacing */
	case CSI_CMD_SVS:
		ret = get_csi_params(arg, &p1, NULL);
		if (ret > 0)
			ls->cell_spacing.row = p1;
		isdb_command_log("Command:CSI: SVS (%d)\n", p1);
		break;
	/* Active Coordinate Position Set */
	case CSI_CMD_ACPS:
		isdb_command_log("Command:CSI: ACPS\n");
		ret = get_csi_params(arg, &p1, &p2);
		if (ret > 0)
			ls->acps[0] = p1;
			ls->acps[1] = p1;
		break;
	default:
		isdb_log("Command:CSI: Unknown command 0x%x\n", *buf);
		break;
	}
	buf++;
	/* ACtual CSI command */
	return buf - buf_pivot;
}

static int parse_command(ISDBSubContext *ctx, const uint8_t *buf, int len)
{
	const uint8_t *buf_pivot = buf;
	uint8_t code_lo = *buf & 0x0f;
	uint8_t code_hi = (*buf & 0xf0) >> 4;
	int ret;
	ISDBSubState *state = &ctx->current_state;
	ISDBSubLayout *ls = &state->layout_state;

	buf++;
	if ( code_hi == 0x00)
	{
		switch(code_lo) {
		/* NUL Control code, which can be added or deleted without effecting to
			information content. */
		case 0x0:
			isdb_command_log("Command: NUL\n");
			break;

		/* BEL Control code used when calling attention (alarm or signal) */
		case 0x7:
			//TODO add bell character here
			isdb_command_log("Command: BEL\n");
			break;

		/**
		 *  APB: Active position goes backward along character path in the length of
		 *	character path of character field. When the reference point of the character
		 *	field exceeds the edge of display area by this movement, move in the
		 *	opposite side of the display area along the character path of the active
		 * 	position, for active position up.
		 */
		case 0x8:
			isdb_command_log("Command: ABP\n");
			break;

		/**
		 *  APF: Active position goes forward along character path in the length of
		 *	character path of character field. When the reference point of the character
		 *	field exceeds the edge of display area by this movement, move in the
		 * 	opposite side of the display area along the character path of the active
		 *	position, for active position down.
		 */
		case 0x9:
			isdb_command_log("Command: APF\n");
			break;

		/**
		 *  APD: Moves to next line along line direction in the length of line direction of
		 *	the character field. When the reference point of the character field exceeds
		 *	the edge of display area by this movement, move to the first line of the
		 *	display area along the line direction.
		 */
		case 0xA:
			isdb_command_log("Command: APD\n");
			break;

		/**
		 * APU: Moves to the previous line along line direction in the length of line
		 *	direction of the character field. When the reference point of the character
		 *	field exceeds the edge of display area by this movement, move to the last
		 *	line of the display area along the line direction.
		 */
		case 0xB:
			isdb_command_log("Command: APU\n");
			break;

		/**
		 * CS: Display area of the display screen is erased.
		 * Specs does not say clearly about whether we have to clear cursor
		 * Need Samples to see whether CS is called after pen move or before it
		 */
		case 0xC:
			isdb_command_log("Command: CS clear Screen\n");
			break;

		/** APR: Active position down is made, moving to the first position of the same
		 *	line.
		 */
		case 0xD:
			isdb_command_log("Command: APR\n");
			break;

		/* LS1: Code to invoke character code set. */
		case 0xE:
			isdb_command_log("Command: LS1\n");
			break;

		/* LS0: Code to invoke character code set. */
		case 0xF:
			isdb_command_log("Command: LS0\n");
			break;
		/* Verify the new version of specs or packet is corrupted */
		default:
			isdb_command_log("Command: Unknown\n");
			break;
		}
	}
	else if (code_hi == 0x01)
	{
		switch(code_lo) {
		/**
		 * PAPF: Active position forward is made in specified times by parameter P1 (1byte).
		 *	Parameter P1 shall be within the range of 04/0 to 07/15 and time shall be
		 *	specified within the range of 0 to 63 in binary value of 6-bit from b6 to b1.
		 *	(b8 and b7 are not used.)
		 */
		case 0x6:
			isdb_command_log("Command: PAPF\n");
			break;

		/**
		 * CAN: From the current active position to the end of the line is covered with
		 *	background colour in the width of line direction in the current character
		 *	field. Active position is not moved.
		 */
		case 0x8:
			isdb_command_log("Command: CAN\n");
			break;

		/* SS2: Code to invoke character code set. */
		case 0x9:
			isdb_command_log("Command: SS2\n");
			break;

		/* ESC:Code for code extension. */
		case 0xB:
			isdb_command_log("Command: ESC\n");
			break;

		/** APS: Specified times of active position down is made by P1 (1 byte) of the first
		 *	parameter in line direction length of character field from the first position
		 *	of the first line of the display area. Then specified times of active position
		 *	forward is made by the second parameter P2 (1 byte) in the character path
		 *	length of character field. Each parameter shall be within the range of 04/0
		 *	to 07/15 and specify time within the range of 0 to 63 in binary value of 6-
		 *	bit from b6 to b1. (b8 and b7 are not used.)
		 */
		case 0xC:
			isdb_command_log("Command: APS\n");
			set_position(ctx, *buf & 0x3F, *(buf+1) & 0x3F);
			buf += 2;
			break;

		/* SS3: Code to invoke character code set. */
		case 0xD:
			isdb_command_log("Command: SS3\n");
			break;

		/**
		 * RS: It is information division code and declares identification and
		 * 	introduction of data header.
		 */
		case 0xE:
			isdb_command_log("Command: RS\n");
			break;

		/**
		 * US: It is information division code and declares identification and
		 *	introduction of data unit.
		 */
		case 0xF:
			isdb_command_log("Command: US\n");
			break;

		/* Verify the new version of specs or packet is corrupted */
		default:
			isdb_command_log("Command: Unknown\n");
			break;
		}
	}
	else if (code_hi == 0x8)
	{
		switch(code_lo) {
		/* BKF */
		case 0x0:
		/* RDF */
		case 0x1:
		/* GRF */
		case 0x2:
		/* YLF */
		case 0x3:
		/* BLF 	*/
		case 0x4:
		/* MGF */
		case 0x5:
		/* CNF */
		case 0x6:
		/* WHF */
		case 0x7:
			isdb_command_log("Command: Forground color (0x%X)\n", Default_clut[code_lo]);
			state->fg_color = Default_clut[code_lo];
			break;

		/* SSZ */
		case 0x8:
			isdb_command_log("Command: SSZ\n");
			ls->font_scale.fscx = 50;
			ls->font_scale.fscy = 50;
			break;

		/* MSZ */
		case 0x9:
			ls->font_scale.fscx = 200;
			ls->font_scale.fscy = 200;
			isdb_command_log("Command: MSZ\n");
			break;

		/* NSZ */
		case 0xA:
			ls->font_scale.fscx = 100;
			ls->font_scale.fscy = 100;
			isdb_command_log("Command: NSZ\n");
			break;

		/* SZX */
		case 0xB:
			isdb_command_log("Command: SZX\n");
			buf++;
			break;

		/* Verify the new version of specs or packet is corrupted */
		default:
			isdb_command_log("Command: Unknown\n");
			break;

		}
	}
	else if ( code_hi == 0x9)
	{
		switch(code_lo) {
		/* COL */
		case 0x0:
			/* Pallete Col */
			if(*buf == 0x20)
			{
				isdb_command_log("Command: COL: Set Clut %d\n",(buf[0] & 0x0F));
				buf++;
				ctx->state.clut_high_idx = (buf[0] & 0x0F);
			}
			else if ((*buf & 0XF0) == 0x40)
			{
				isdb_command_log("Command: COL: Set Forground 0x%08X\n", Default_clut[*buf & 0x0F]);
				ctx->current_state.fg_color = Default_clut[*buf & 0x0F];
			}
			else if ((*buf & 0XF0) == 0x50)
			{
				isdb_command_log("Command: COL: Set Background 0x%08X\n", Default_clut[*buf & 0x0F]);
				ctx->current_state.bg_color = Default_clut[*buf & 0x0F];
			}
			else if ((*buf & 0XF0) == 0x60)
			{
				isdb_command_log("Command: COL: Set half Forground 0x%08X\n", Default_clut[*buf & 0x0F]);
				ctx->current_state.hfg_color = Default_clut[*buf & 0x0F];
			}
			else if ((*buf & 0XF0) == 0x70)
			{
				isdb_command_log("Command: COL: Set Half Background 0x%8X\n", Default_clut[*buf & 0x0F]);
				ctx->current_state.hbg_color = Default_clut[*buf & 0x0F];
			}

			buf++;
			break;

		/* FLC */
		case 0x1:
			isdb_command_log("Command: FLC\n");
			buf++;
			break;

		/* CDC */
		case 0x2:
			isdb_command_log("Command: CDC\n");
			buf++;
			buf++;
			buf++;
			break;

		/* POL */
		case 0x3:
			isdb_command_log("Command: POL\n");
			buf++;
			break;

		/* WMM */
		case 0x4:
			isdb_command_log("Command: WMM\n");
			buf++;
			buf++;
			buf++;
			break;

		/* MACRO */
		case 0x5:
			isdb_command_log("Command: MACRO\n");
			buf++;
			break;

		/* HLC */
		case 0x7:
			isdb_command_log("Command: HLC\n");
			buf++;
			break;

		/* RPC */
		case 0x8:
			isdb_command_log("Command: RPC\n");
			buf++;
			break;

		/* SPL */
		case 0x9:
			isdb_command_log("Command: SPL\n");
			break;

		/* STL */
		case 0xA:
			isdb_command_log("Command: STL\n");
			break;

		/* CSI Code for code system extension indicated*/
		case 0xB:
			ret = parse_csi(ctx, buf, len);
			buf += ret;
			break;

		/* TIME */
		case 0xD:
			isdb_command_log("Command: TIME\n");
			buf++;
			buf++;
			break;

		/* Verify the new version of specs or packet is corrupted */
		default:
			isdb_command_log("Command: Unknown\n");
			break;
		}
	}

	return buf - buf_pivot;

}

static int parse_caption_management_data(ISDBSubContext *ctx, const uint8_t *buf, int size)
{
	const uint8_t *buf_pivot = buf;
	int i;

	ctx->tmd = (*buf >> 6);
	isdb_log("CC MGMT DATA: TMD: %d\n", ctx->tmd);
	buf++;

	if (ctx->tmd == ISDB_TMD_FREE)
	{
		isdb_log("Playback time is not restricted to synchronize to the clock.\n");
	}
	else if (ctx->tmd == ISDB_TMD_OFFSET_TIME)
	{
		/**
		 * This 36-bit field indicates offset time to add to the playback time when the
		 * clock control mode is in offset time mode. Offset time is coded in the
		 * order of hour, minute, second and millisecond, using nine 4-bit binary
                 * coded decimals (BCD).
		 * 
		 * +-----------+-----------+---------+--------------+
		 * |  hour     |   minute  |   sec   |  millisecond |
		 * +-----------+-----------+---------+--------------+
		 * |  2 (4bit) | 2 (4bit)  | 2 (4bit)|    3 (4bit)  |
		 * +-----------+-----------+---------+--------------+
		 */
		ctx->offset_time.hour = ((*buf>>4) * 10) + (*buf&0xf);
		buf++;
		ctx->offset_time.min = ((*buf>>4) * 10) + (*buf&0xf);
		buf++;
		ctx->offset_time.sec = ((*buf>>4) * 10) + (*buf&0xf);
		buf++;
		ctx->offset_time.milli = ((*buf>>4) * 100) + ( (*buf&0xf) * 10) + (buf[1]&0xf);
		buf += 2;
		isdb_log("CC MGMT DATA: OTD( h:%d m:%d s:%d millis: %d\n",
			ctx->offset_time.hour, ctx->offset_time.min,
			ctx->offset_time.sec, ctx->offset_time.milli);
		

	}
	else
	{
		isdb_log("Playback time is in accordance with the time of the clock,"
			"which is calibrated by clock signal (TDT). Playback time is"
			"given by PTS.\n");
	}
	ctx->nb_lang = *buf;
	isdb_log("CC MGMT DATA: nb languages: %d\n", ctx->nb_lang);
	buf++;

	for (i = 0; i < ctx->nb_lang; i++)
	{
		isdb_log("CC MGMT DATA: %d\n", (*buf&0x1F) >> 5);
		ctx->dmf = *buf&0x0F;
		isdb_log("CC MGMT DATA: DMF 0x%X\n", ctx->dmf);
		buf++;
		if (ctx->dmf == 0xC || ctx->dmf == 0xD|| ctx->dmf == 0xE)
		{
			ctx->dc = *buf;
			if(ctx->dc == 0x00)
				isdb_log("Attinuation Due to Rain\n");
		}
		isdb_log("CC MGMT DATA: languages: %c%c%c\n", buf[0], buf[1], buf[2]);
		buf += 3;
		isdb_log("CC MGMT DATA: Format: 0x%X\n", *buf>>4);
		/* 8bit code is not used by brazilians Arib they use utf-8*/
		isdb_log("CC MGMT DATA: TCS: 0x%X\n", (*buf>>2)&0x3);
		isdb_log("CC MGMT DATA: Rollup mode: 0x%X\n", *buf&0x3);
	} 
	return buf - buf_pivot;
}

static int parse_statement(ISDBSubContext *ctx, const uint8_t *buf, int size)
{
	const uint8_t *buf_pivot = buf;
	int ret;

	while( (buf - buf_pivot) < size)
	{
		unsigned char code    = (*buf & 0xf0) >> 4;
		unsigned char code_lo = *buf & 0x0f;
		if (code <= 0x1)
			ret = parse_command(ctx, buf, size);
		/* Special case *1(SP) */
		else if ( code == 0x2 && code_lo == 0x0 )
			ret = append_char(ctx,buf[0] );
		/* Special case *3(DEL) */
		else if ( code == 0x7 && code_lo == 0xF )
			/*TODO DEL should have block in fg color */
			ret = append_char(ctx, buf[0]);
		else if ( code <= 0x7)
			ret = append_char(ctx, buf[0]);
		else if ( code <= 0x9)
			ret = parse_command(ctx, buf, size);
		/* Special case *2(10/0) */
		else if ( code == 0xA && code_lo == 0x0 )
			/*TODO handle */;
		/* Special case *4(15/15) */
		else if (code == 0x0F && code_lo == 0XF )
			/*TODO handle */;
		else
			ret = append_char(ctx, buf[0]);
		if (ret < 0)
			break;
		buf += ret;
	}
	return 0;
}

static int parse_data_unit(ISDBSubContext *ctx,const uint8_t *buf, int size)
{
	int unit_parameter;
	int len;
	/* unit seprator */
	buf++;

	unit_parameter = *buf++;
	len = RB24(buf);
	buf += 3;
	switch(unit_parameter)
	{
		/* statement body */
		case 0x20:
			parse_statement(ctx, buf, len);
	}
	return 0;
}

static int parse_caption_statement_data(ISDBSubContext *ctx, int lang_id, const uint8_t *buf, int size)
{
	int tmd;
	int len;
	int ret;

	tmd = *buf >> 6;
	buf++;
	/* skip timing data */
	if (tmd == 1 || tmd == 2)
		buf += 5;

	len = RB24(buf);
	buf += 3;
	ret = parse_data_unit(ctx, buf, len);
	if( ret < 0)
		return -1;
	return 0;
}

/** Acc to http://www.bocra.org.bw/sites/default/files/documents/Appendix%201%20-%20Operational%20Guideline%20for%20ISDB-Tbw.pdf
 * In table AP8-1 there are modification to ARIB TR-B14 in volume 3 Section 2 4.4.1 character encoding is UTF-8
 * instead of 8 bit character, just now we dont have any means to detect which country this video is
 * therefor we have hardcoded UTF-8 as encoding
 */
int isdb_parse_data_group(void *codec_ctx,const uint8_t *buf, struct cc_subtitle *sub)
{
	ISDBSubContext *ctx = codec_ctx;
	const uint8_t *buf_pivot = buf;
	int id = (*buf >> 2);
	int version = (*buf & 2);
	int link_number = 0;
	int last_link_number = 0;
	int group_size = 0;
	int ret = 0;


	if ( (id >> 4) == 0 )
	{
		isdb_log("ISDB group A\n");
	}
	else if ( (id >> 4) == 0 )
	{
		isdb_log("ISDB group B\n");
	}

	isdb_log("ISDB (Data group) version %d\n",version);

	buf++;
	link_number = *buf++;
	last_link_number = *buf++;
	isdb_log("ISDB (Data group) link_number %d last_link_number %d\n", link_number, last_link_number);

	group_size = RB16(buf);
	buf += 2;
	isdb_log("ISDB (Data group) group_size %d\n", group_size);

	if (ctx->prev_timestamp > ctx->timestamp)
	{
		ctx->prev_timestamp = ctx->timestamp;
	}
	if((id & 0x0F) == 0)
	{
		/* Its Caption management */
		ret = parse_caption_management_data(ctx, buf, group_size);
	}
	else if ((id & 0x0F) < 8 )
	{
		/* Caption statement data */
		isdb_log("ISDB %d language\n",id);
		ret = parse_caption_statement_data(ctx, id & 0x0F, buf, group_size);
	}
	else
	{
		/* This is not allowed in spec */
	}
	if(ret < 0)
		return -1;
	buf += group_size;

	/* Copy data if there in buffer */
	if (ctx->text.len > 0 )
	{
		add_cc_sub_text(sub, ctx->text.buf, ctx->prev_timestamp, ctx->timestamp, "NA", "ISDB", CCX_ENC_UTF_8);
		ctx->text.used = 0;
		ctx->text.len = 0;
		if (sub->start_time == sub->end_time)
			sub->end_time += 2;
		ctx->prev_timestamp = ctx->timestamp;
	}
	//TODO check CRC
	buf += 2;

	return buf - buf_pivot;
}

int isdbsub_decode(struct lib_cc_decode *dec_ctx, const uint8_t *buf, size_t buf_size, struct cc_subtitle *sub)
{
	const uint8_t *header_end = NULL;
	int ret = 0;
	ISDBSubContext *ctx = dec_ctx->private_data;
	if(*buf++ != 0x80)
	{
		mprint("\nNot a Syncronised PES\n");
		return -1;
	}
	/* private data stream = 0xFF */
	buf++;
	header_end = buf + (*buf & 0x0f);
	buf++;
	while (buf < header_end)
	{
	/* TODO find in spec what is header */
		buf++;
	}
	ret = isdb_parse_data_group(ctx, buf, sub);
	if (ret < 0)
		return -1;


	return 1;
}
int isdb_set_global_time(struct lib_cc_decode *dec_ctx, uint64_t timestamp)
{
	ISDBSubContext *ctx = dec_ctx->private_data;
	ctx->timestamp = timestamp;
	return CCX_OK;
}