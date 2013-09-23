/**
 * LTTng to GTKwave trace conversion
 *
 * Authors:
 * Ivan Djelic <ivan.djelic@parrot.com>
 * Matthieu Castet <matthieu.castet@parrot.com>
 *
 * Copyright (C) 2013 Parrot S.A.
 */
#ifndef SAVEFILE_H
#define SAVEFILE_H 1

/* Additional GTKwave stuff (trace flags) */

enum TraceEntFlagBits {
	TR_HIGHLIGHT_B,
	TR_HEX_B,
	TR_DEC_B,
	TR_BIN_B,
	TR_OCT_B,
	TR_RJUSTIFY_B,
	TR_INVERT_B,
	TR_REVERSE_B,
	TR_EXCLUDE_B,
	TR_BLANK_B,
	TR_SIGNED_B,
	TR_ASCII_B,
	TR_COLLAPSED_B,
	TR_FTRANSLATED_B,
	TR_PTRANSLATED_B,
	TR_ANALOG_STEP_B,
	TR_ANALOG_INTERPOLATED_B,
	TR_ANALOG_BLANK_STRETCH_B,
	TR_REAL_B,
	TR_ANALOG_FULLSCALE_B,
	TR_ZEROFILL_B,
	TR_ONEFILL_B,
	TR_CLOSED_B,
	TR_GRP_BEGIN_B,
	TR_GRP_END_B,
	TR_BINGRAY_B,
	TR_GRAYBIN_B
};

#define TR_HIGHLIGHT              (1 << TR_HIGHLIGHT_B)
#define TR_HEX                    (1 << TR_HEX_B)
#define TR_ASCII                  (1 << TR_ASCII_B)
#define TR_DEC                    (1 << TR_DEC_B)
#define TR_BIN                    (1 << TR_BIN_B)
#define TR_OCT                    (1 << TR_OCT_B)
#define TR_RJUSTIFY               (1 << TR_RJUSTIFY_B)
#define TR_INVERT                 (1 << TR_INVERT_B)
#define TR_REVERSE                (1 << TR_REVERSE_B)
#define TR_EXCLUDE                (1 << TR_EXCLUDE_B)
#define TR_BLANK                  (1 << TR_BLANK_B)
#define TR_SIGNED                 (1 << TR_SIGNED_B)
#define TR_ANALOG_STEP            (1 << TR_ANALOG_STEP_B)
#define TR_ANALOG_INTERPOLATED    (1 << TR_ANALOG_INTERPOLATED_B)
#define TR_ANALOG_BLANK_STRETCH   (1 << TR_ANALOG_BLANK_STRETCH_B)
#define TR_REAL                   (1 << TR_REAL_B)
#define TR_ANALOG_FULLSCALE	(1 << TR_ANALOG_FULLSCALE_B)
#define TR_ZEROFILL		(1 << TR_ZEROFILL_B)
#define TR_ONEFILL		(1 << TR_ONEFILL_B)
#define TR_CLOSED		(1 << TR_CLOSED_B)

#define TR_GRP_BEGIN		(1 << TR_GRP_BEGIN_B)
#define TR_GRP_END		(1 << TR_GRP_END_B)
#define TR_GRP_MASK		(TR_GRP_BEGIN|TR_GRP_END)

#define TR_BINGRAY		(1 << TR_BINGRAY_B)
#define TR_GRAYBIN		(1 << TR_GRAYBIN_B)
#define TR_GRAYMASK		(TR_BINGRAY|TR_GRAYBIN)

#define TR_NUMMASK    (TR_ASCII|TR_HEX|TR_DEC|TR_BIN|TR_OCT|TR_SIGNED|TR_REAL)

#define TR_COLLAPSED              (1 << TR_COLLAPSED_B)
#define TR_ISCOLLAPSED            (TR_BLANK|TR_COLLAPSED)

#define TR_FTRANSLATED            (1 << TR_FTRANSLATED_B)
#define TR_PTRANSLATED            (1 << TR_PTRANSLATED_B)

#define TR_ANALOGMASK             (TR_ANALOG_STEP|TR_ANALOG_INTERPOLATED)
#endif
