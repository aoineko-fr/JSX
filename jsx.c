// ____________________________
// ██▀▀█▀▀██▀▀▀▀▀▀▀█▀▀█        │   ▄▄▄                ▄▄      
// ██  ▀  █▄  ▀██▄ ▀ ▄█ ▄▀▀ █  │  ▀█▄  ▄▀██ ▄█▄█ ██▀▄ ██  ▄███
// █  █ █  ▀▀  ▄█  █  █ ▀▄█ █▄ │  ▄▄█▀ ▀▄██ ██ █ ██▀  ▀█▄ ▀█▄▄
// ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀────────┘                 ▀▀
//  Program template
//─────────────────────────────────────────────────────────────────────────────

//=============================================================================
// INCLUDES
//=============================================================================
#include "msxgl.h"

//=============================================================================
// DEFINES
//=============================================================================

// Library's logo
#define MSX_GL			"\x01\x02\x03\x04\x05\x06"
#define SCREEN_W		512
#define OUTPUT_Y		18
#define INPUT_Y			95
#define SELECT_B		0x40

struct InputPin
{
	const c8* Name;
	u8 Bit;
	u8 Y;
};

//=============================================================================
// READ-ONLY DATA
//=============================================================================

// Fonts data
#include "font/font_mgl_sample6.h"

// Sign-of-life character animation
const u8 g_ChrAnim[] = { '-', '/', '|', '\\' };

//-----------------------------------------------------------------------------
// R#14   I/O Parallel Port A (ready)
//-----------------------------------------------------------------------------
//	7	6	5	4	3	2	1	0	
//	CR	JIS	P7	P6	P4	P3	P2	P1 	
//  │	│	│	│	│	│	│	└── Pin 1 state of the selected general port (Up if joystick)
//	│	│	│	│	│   │   └────── Pin 2 state of the selected general port (Down if joystick)
//	│	│	│	│	│   └────────── Pin 3 state of the selected general port (Left if joystick)
//	│	│	│	│	└────────────── Pin 4 state of the selected general port (Right if joystick)
//	│	│	│	└────────────────── Pin 6 state of the selected general port (Trigger A if joystick)
//	│	│	└────────────────────── Pin 7 state of the selected general port (Trigger B if joystick)
//	│	└────────────────────────── 1 for JIS keyboard, 0 for JP50on (only valid for Japanese MSX)
//	└────────────────────────────── CASRD (Reading signal on cassette)

// Input pin data table
const struct InputPin g_InputPins[] = 
{
	{ "Pin 1", 0b00000001, INPUT_Y + 21 + 16 * 0 },	
	{ "Pin 2", 0b00000010, INPUT_Y + 21 + 16 * 1 },	
	{ "Pin 3", 0b00000100, INPUT_Y + 21 + 16 * 2 },	
	{ "Pin 4", 0b00001000, INPUT_Y + 21 + 16 * 3 },	
	{ "Pin 6", 0b00010000, INPUT_Y + 21 + 16 * 4 },	
	{ "Pin 7", 0b00100000, INPUT_Y + 21 + 16 * 5 },	
};

//-----------------------------------------------------------------------------
// R#15   I/O Parallel Port B (write)
//-----------------------------------------------------------------------------
//	7	6	5	4	3	2	1	0	
//	LED	SEL	B8	A8	B7	B6	A7	A6 	
//  │	│	│	│	│	│	│	└── Pin control 6 of the general port 1
//	│	│	│	│	│   │   └────── Pin control 7 of the general port 1
//	│	│	│	│	│   └────────── Pin control 6 of the general port 2
//	│	│	│	│	└────────────── Pin control 7 of the general port 2
//	│	│	│	└────────────────── Pin control 8 of the general port 1 (0 for standard joystick mode)
//	│	│	└────────────────────── Pin control 8 of the general port 2 (0 for standard joystick mode)
//	│	└────────────────────────── Selection of the general port readable via register 14 (1 for port 2)
//	└────────────────────────────── LED control of the "Code" or "Kana" key. (1 to turn off)

const struct InputPin g_OutputPinsA[] = 
{
	{ "Pin 6", 0b00000001, OUTPUT_Y + 33 + 16 * 0 },	
	{ "Pin 7", 0b00000010, OUTPUT_Y + 33 + 16 * 1 },	
	{ "Pin 8", 0b00010000, OUTPUT_Y + 33 + 16 * 2 },	
};

const struct InputPin g_OutputPinsB[] = 
{
	{ "Pin 6", 0b00000100, OUTPUT_Y + 33 + 16 * 0 },	
	{ "Pin 7", 0b00001000, OUTPUT_Y + 33 + 16 * 1 },	
	{ "Pin 8", 0b00100000, OUTPUT_Y + 33 + 16 * 2 },	
};

// Cursor sprite pattern
const u8 g_CursorPattern[8] =
{
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
};

//=============================================================================
// VARIABLES
//=============================================================================

u8 g_VBlank = 0;		// V-blank synch flag
u8 g_Frame = 0;			// Frame counter

u8 g_R14;				// PSG 
u8 g_R15;
const struct InputPin* g_Output;
bool g_Synch = TRUE;

bool g_AutoPulse6 = FALSE;
bool g_AutoPulse7 = FALSE;
bool g_AutoPulse8 = FALSE;

//=============================================================================
// MAIN LOOP
//=============================================================================

//-----------------------------------------------------------------------------
//
void Write_R15(u8 val) __FASTCALL
{
	val; // L
	__asm
		ld		a, #15
		out		(P_PSG_REGS), a
		ld		a, l
		out		(P_PSG_DATA), a			// Write port B value
	__endasm;
}

//-----------------------------------------------------------------------------
//
u8 Read_R14()
{
	__asm
		ld		a, #14
		out		(P_PSG_REGS), a
		in		a, (P_PSG_STAT)			// Read port A value
	__endasm;
}

//-----------------------------------------------------------------------------
//
void SelectPort(u8 port)
{
	if(port == 0)
	{
		g_R15 &= ~SELECT_B;
		Print_DrawTextAt(36, OUTPUT_Y + 12, "A");
		g_Output = g_OutputPinsA;
	}
	else
	{
		g_R15 |= SELECT_B;
		Print_DrawTextAt(36, OUTPUT_Y + 12, "B");
		g_Output = g_OutputPinsB;
	}
}

//-----------------------------------------------------------------------------
//
void TogglePin(u8 pin)
{
	u8 out = g_Output[pin].Bit;
	if(g_R15 & out)
		g_R15 &= ~out;
	else
		g_R15 |= out;
}

//-----------------------------------------------------------------------------
//
void HigherPin(u8 pin)
{
	u8 out = g_Output[pin].Bit;
	g_R15 |= out;
}

//-----------------------------------------------------------------------------
//
void LowerPin(u8 pin)
{
	u8 out = g_Output[pin].Bit;
	g_R15 &= ~out;
}

//-----------------------------------------------------------------------------
//
void VDP_InterruptHandler()
{
	g_VBlank = 1;
}

//-----------------------------------------------------------------------------
// Wait for V-Blank period
void WaitVBlank()
{
	while(g_VBlank == 0) {}
	g_VBlank = 0;
	g_Frame++;
}

//-----------------------------------------------------------------------------
// Program entry point
void main()
{
	// Initialize screen display
	VDP_SetMode(VDP_MODE_SCREEN7);
	VDP_SetColor(COLOR_BLACK);
	VDP_SetLineCount(VDP_LINE_212);
	VDP_EnableVBlank(TRUE);
	VDP_ClearVRAM();
	VDP_CommandHMMV(0, 0, SCREEN_W, 212, COLOR_BLACK << 4 | COLOR_BLACK);
	VDP_SetPaletteEntry(COLOR_GRAY, RGB16(2, 2, 2));

	// Initialize sprite
	VDP_EnableSprite(TRUE);
	VDP_SetSpriteFlag(VDP_SPRITE_SIZE_8 | VDP_SPRITE_SCALE_1);
	VDP_LoadSpritePattern(g_CursorPattern, 0, 1);

	// Initialize font
	Print_SetBitmapFont(g_Font_MGL_Sample6);
	Print_SetColor(COLOR_WHITE, COLOR_BLACK);

	// Draw information
	Print_DrawTextAt(0, 0, MSX_GL" JSX");
	VDP_CommandLMMV(0, 12, SCREEN_W, 1, COLOR_GRAY, 0);

	Print_SetColor(COLOR_MEDIUM_RED, COLOR_BLACK);
	Print_DrawTextAt(0, OUTPUT_Y, "[OUTPUT]");
	Print_SetColor(COLOR_LIGHT_RED, COLOR_BLACK);
	Print_DrawTextAt(4, OUTPUT_Y + 12, "Port:");
	u8 y = OUTPUT_Y + 12 + 12;
	loop(i, 3)
	{
		Print_DrawTextAt(4, y, g_OutputPinsA[i].Name);
		VDP_CommandLMMV(0, y + 13, SCREEN_W, 1, COLOR_DARK_BLUE, 0);
		VDP_SetSpriteExUniColor(i + 6, 0, y + 7, 0, COLOR_WHITE);
		y += 16;
	}

	Print_SetColor(COLOR_MEDIUM_GREEN, COLOR_BLACK);
	Print_DrawTextAt(0, INPUT_Y, "[INPUT]");
	Print_SetColor(COLOR_LIGHT_GREEN, COLOR_BLACK);
	y = INPUT_Y + 12;
	loop(i, 6)
	{
		Print_DrawTextAt(4, y, g_InputPins[i].Name);
		VDP_CommandLMMV(0, y + 13, SCREEN_W, 1, COLOR_DARK_BLUE, 0);
		VDP_SetSpriteExUniColor(i, 0, y + 7, 0, COLOR_WHITE);
		y += 16;
	}

	Print_SetColor(COLOR_GRAY, COLOR_BLACK);
	Print_DrawTextAt(0, 204, "0:Toggle synch. 1,2: Select port. 6,7,8:Toggle pin. +Ctrl:Auto pulse");

	Print_SetColor(COLOR_LIGHT_RED, COLOR_BLACK);
	SelectPort(0);

	g_R15 = 0x00;
	u8 prevRow0 = 0xFF;
	u8 prevRow1 = 0xFF;
	u16 count = 0;

	while(1)
	{
		// Wait for V-Blank
		if (g_Synch)
			WaitVBlank();

		// Print_SetPosition(255-8, 0);
		// Print_DrawChar(g_ChrAnim[ % 4]);

		//.....................................................................
		// Handle input
		u8 row0 = Keyboard_Read(0);
		u8 row1 = Keyboard_Read(1);
		u8 row6 = Keyboard_Read(6);
		
		// Toggle v-synch
		if(IS_KEY_PRESSED(row0, KEY_0) && !IS_KEY_PRESSED(prevRow0, KEY_0))
			g_Synch = !g_Synch;

		// Select joystick port
		if(IS_KEY_PRESSED(row0, KEY_1) && !IS_KEY_PRESSED(prevRow0, KEY_1))
			SelectPort(0);
		else if(IS_KEY_PRESSED(row0, KEY_2) && !IS_KEY_PRESSED(prevRow0, KEY_2))
			SelectPort(1);

		// Toggle output pins
		if(IS_KEY_PRESSED(row0, KEY_6) && !IS_KEY_PRESSED(prevRow0, KEY_6))
		{
			if(IS_KEY_PRESSED(row6, KEY_CTRL))
			{
				if (g_AutoPulse6)
					LowerPin(0);
				g_AutoPulse6 = !g_AutoPulse6;
			}
			else
			{
				TogglePin(0);
				g_AutoPulse6 = FALSE;
			}
		}
		if(IS_KEY_PRESSED(row0, KEY_7) && !IS_KEY_PRESSED(prevRow0, KEY_7))
		{
			if(IS_KEY_PRESSED(row6, KEY_CTRL))
			{
				if (g_AutoPulse7)
					LowerPin(1);
				g_AutoPulse7 = !g_AutoPulse7;
			}
			else
			{
				TogglePin(1);
				g_AutoPulse7 = FALSE;
			}
		}
		if(IS_KEY_PRESSED(row1, KEY_8) && !IS_KEY_PRESSED(prevRow1, KEY_8))
		{
			if(IS_KEY_PRESSED(row6, KEY_CTRL))
			{
				if (g_AutoPulse8)
					LowerPin(2);
				g_AutoPulse8 = !g_AutoPulse8;
			}
			else
			{
				TogglePin(2);
				g_AutoPulse8 = FALSE;
			}
		}

		// Apply auto-pulse
		if (g_AutoPulse6)
			TogglePin(0);
		if (g_AutoPulse7)
			TogglePin(1);
		if (g_AutoPulse8)
			TogglePin(2);

		prevRow0 = row0;
		prevRow1 = row1;

		//.....................................................................
		// Update PSG register values

		// Write R#15
		Write_R15(g_R15);

		// Read R#14
		g_R14 = Read_R14();

		// Print_SetPosition(8*6, OUTPUT_Y);
		// Print_DrawHex8(g_R15);
		// Print_SetPosition(7*6, INPUT_Y);
		// Print_DrawHex8(g_R14);

		//.....................................................................
		// Display histogram

		g_VDP_Command.DX = count;
		g_VDP_Command.NX = 1; 
		g_VDP_Command.NY = 4; 
		g_VDP_Command.ARG = 0; 
		g_VDP_Command.CMD = VDP_CMD_LMMV;

		u8 y = INPUT_Y + 12;
		u16 addr = 	g_SpriteAtributeLow + 1; // X-coordinate of sprite index 0
		const struct InputPin* pins = g_InputPins;
		loop(i, 6)
		{
			bool high = g_R14 & pins->Bit;
			g_VDP_Command.DY = pins->Y; 
			g_VDP_Command.CLR = high ? COLOR_LIGHT_BLUE : COLOR_BLACK; 
			VPD_CommandSetupR36(); // Start LMMV command
			VDP_Poke(count / 2, addr, g_SpriteAtributeHigh);
			addr += 4;
			y += 16;
			pins++;
		}

		y = OUTPUT_Y + 12 + 12;
		pins = g_Output;
		loop(i, 3)
		{
			bool high = g_R15 & pins->Bit;
			g_VDP_Command.DY = pins->Y; 
			g_VDP_Command.CLR = high ? COLOR_LIGHT_BLUE : COLOR_BLACK; 
			VPD_CommandSetupR36(); // Start LMMV command
			VDP_Poke(count / 2, addr, g_SpriteAtributeHigh);
			addr += 4;
			y += 16;
			pins++;
		}

		count++;
	}
}