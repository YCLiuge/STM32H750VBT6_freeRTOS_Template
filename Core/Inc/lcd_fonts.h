#ifndef __LCD_FONTS_H
#define __LCD_FONTS_H

#include <stdint.h>


// Definition of Font Related Structures;
typedef struct _pFont
{    
	const uint8_t 		*pTable;  		// Font array address;
	uint16_t 			Width; 		 	// The font width of a single character;
	uint16_t 			Height; 		// The font height of a single character;
	uint16_t 			Sizes;	 		// The number of font data for a single character
	uint16_t			Table_Rows;		// This parameter is only used for Chinese character models and represents the row size of a two-dimensional array;
} pFONT;


/*------------------------------------ CHN character ---------------------------------------------*/

extern	pFONT	CH_Font12 ;	// 1212 font;
extern	pFONT	CH_Font16 ;	// 1616 font;
extern	pFONT	CH_Font20 ;	// 2020 font;
extern	pFONT	CH_Font24 ;	// 2424 font;
extern	pFONT	CH_Font32 ;	// 3232 font;


/*------------------------------------ ASCII character ---------------------------------------------*/

extern pFONT ASCII_Font32;	// 3216 font;
extern pFONT ASCII_Font24;	// 2412 font;
extern pFONT ASCII_Font20; 	// 2010 font;
extern pFONT ASCII_Font16; 	// 1608 font;
extern pFONT ASCII_Font12; 	// 1206 font;

#endif 
 
