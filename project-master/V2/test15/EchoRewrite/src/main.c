#include "xaxidma.h"
#include "xplatform_info.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "xil_util.h"
#include "xscugic.h"
#include "xscugic_hw.h"
#include "math.h"
#include <stdio.h>
#include "xparameters.h"
#include "netif/xadapter.h"
#include "echo.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "lwip/tcp.h"
#include "xil_cache.h"
#include "lwip/dhcp.h"




/*#if defined(__arm__) && !defined(ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
int ProgramSi5324(void);
int ProgramSfpPhy(void);
#endif
#endif



/************************** Constant Definitions *****************************/
#define NUM_CHANNELS 4 // when adapting, also adapt DMA_Instances and TxDoneA and LED_BUFFER_Instances
// Buffer Declaration
#define BUFFER_SIZE 1024 // number of leds
#define MATRIX_SIZE 64
#define DMA_TRANSFER_SIZE (BUFFER_SIZE * sizeof(uint32_t))

// Interrupt IDs for each DMA channel
#define XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR 61
#define XPAR_FABRIC_AXI_DMA_1_MM2S_INTROUT_INTR 62
#define XPAR_FABRIC_AXI_DMA_2_MM2S_INTROUT_INTR 63
#define XPAR_FABRIC_AXI_DMA_3_MM2S_INTROUT_INTR 64

// Static variables for the interrupt controller
static XScuGic intc;
static XScuGic_Config *intc_cfg_ptr;

/************************** Variable Definitions *****************************/
// Device instances for each DMA channel
XAxiDma DMA_CH1_inst;
XAxiDma DMA_CH2_inst;
XAxiDma DMA_CH3_inst;
XAxiDma DMA_CH4_inst;
XAxiDma *DMA_Instances[NUM_CHANNELS] = {&DMA_CH1_inst, &DMA_CH2_inst, &DMA_CH3_inst, &DMA_CH4_inst};

uint32_t ST_Result;
uint32_t ST_Result_Cfg;

/************************** DMA Interrupt Handler *****************************/
// Interrupt handlers for each channel
void DMA_Interrupt_Handler_Ch1(void *CallbackRef);
void DMA_Interrupt_Handler_Ch2(void *CallbackRef);
void DMA_Interrupt_Handler_Ch3(void *CallbackRef);
void DMA_Interrupt_Handler_Ch4(void *CallbackRef);

// Flags interrupt handlers use to notify the application context the events.

volatile int TxDoneA[NUM_CHANNELS] = {0};

/************************** LED Buffer Definitions  ******************************/
uint32_t LED_BUFFER_CH1[BUFFER_SIZE] __attribute__((aligned(32))) __attribute__((section (".noncached_buffer")));
uint32_t LED_BUFFER_CH2[BUFFER_SIZE] __attribute__((aligned(32))) __attribute__((section (".noncached_buffer")));
uint32_t LED_BUFFER_CH3[BUFFER_SIZE] __attribute__((aligned(32))) __attribute__((section (".noncached_buffer")));
uint32_t LED_BUFFER_CH4[BUFFER_SIZE] __attribute__((aligned(32))) __attribute__((section (".noncached_buffer")));
uint32_t *LED_BUFFER_Instances[NUM_CHANNELS] = {LED_BUFFER_CH1, LED_BUFFER_CH2, LED_BUFFER_CH3, LED_BUFFER_CH4};

/************************** DMA Initialization Function **************************/
// Function to initialize and configure four DMA channels and enable IRQ interrupts
void DMA_INIT()
{
	// Channel 1
	XAxiDma_Config *DMA_CH1_cfg = XAxiDma_LookupConfig(XPAR_AXI_DMA_CH1_DEVICE_ID);
	ST_Result_Cfg = XAxiDma_CfgInitialize(&DMA_CH1_inst, DMA_CH1_cfg);
	//XAxiDma_IntrEnable(&DMA_CH1_inst, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

	// Channel 2
	XAxiDma_Config *DMA_CH2_cfg = XAxiDma_LookupConfig(XPAR_AXI_DMA_CH2_DEVICE_ID);
	XAxiDma_CfgInitialize(&DMA_CH2_inst, DMA_CH2_cfg);
	//XAxiDma_IntrEnable(&DMA_CH2_inst, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

	// Channel 3
	XAxiDma_Config *DMA_CH3_cfg = XAxiDma_LookupConfig(XPAR_AXI_DMA_CH3_DEVICE_ID);
	XAxiDma_CfgInitialize(&DMA_CH3_inst, DMA_CH3_cfg);
	//XAxiDma_IntrEnable(&DMA_CH3_inst, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

	// Channel 4
	XAxiDma_Config *DMA_CH4_cfg = XAxiDma_LookupConfig(XPAR_AXI_DMA_CH4_DEVICE_ID);
	XAxiDma_CfgInitialize(&DMA_CH4_inst, DMA_CH4_cfg);
	//XAxiDma_IntrEnable(&DMA_CH4_inst, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
}

// Function to clear the buffer
void Clear_Buffer(uint32_t *buffer)
{
	for (int i = 0; i < BUFFER_SIZE; ++i)
	{
		buffer[i] = 0;
	}
}

const uint16_t gamma_lut[256] = {
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
     1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   4,   4,
     4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,
     9,   9,  10,  10,  11,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,
    16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  22,  22,  23,  23,  24,  25,
    25,  26,  26,  27,  28,  28,  29,  30,  30,  31,  32,  33,  33,  34,  35,  36,
    36,  37,  38,  39,  39,  40,  41,  42,  43,  43,  44,  45,  46,  47,  48,  49,
    49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  63,
    65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  81,
    82,  83,  84,  85,  86,  87,  89,  90,  91,  92,  93,  95,  96,  97,  98, 100,
   101, 102, 103, 105, 106, 107, 108, 110, 111, 112, 114, 115, 116, 118, 119, 121,
   122, 123, 125, 126, 128, 129, 130, 132, 133, 135, 136, 138, 139, 141, 142, 144,
   145, 147, 148, 150, 151, 153, 154, 156, 157, 159, 161, 162, 164, 165, 167, 169,
   170, 172, 174, 175, 177, 179, 180, 182, 184, 185, 187, 189, 191, 192, 194, 196,
   198, 199, 201, 203, 205, 206, 208, 210, 212, 214, 216, 217, 219, 221, 223, 225,
   227, 229, 231, 232, 234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 256,
  };


uint32_t led_matrixol[32][32] = {};
uint32_t led_matrixor[32][32] = {};
uint32_t led_matrixur[32][32] = {};
uint32_t led_matrixul[32][32] = {};

uint32_t Gamma_LUT_Init(int row, int col, int index)
{
	uint32_t HexCol = 0;

	if (index == 0)
	{
		HexCol = led_matrixol[row][col];
	}
	if (index == 1)
	{
		HexCol = led_matrixor[row][col];
	}
	if (index == 2)
	{
		HexCol = led_matrixor[row][col];
	}
	if (index == 3)
	{
		HexCol = led_matrixur[row][col];
	}

	uint32_t b = (uint32_t)(((HexCol >> 0) & 0xFF));
	uint32_t r = (uint32_t)(((HexCol >> 8) & 0xFF));
	uint32_t g = (uint32_t)(((HexCol >> 16) & 0xFF));

	b = gamma_lut[b];
	r = gamma_lut[r];
	g = gamma_lut[g];

	return (HexCol & 0xFF0000000 | (g << 16) | (r << 8) | (b << 0));
}

int process_matrix_section(uint32_t *buffer, int i, int row_start, int row_end, int col_start, int col_end, int index)
{
	for (int row = row_start; row < row_end; row++)
	{
		for (int col = col_start; col < col_end; col++)
		{
			// Bestimmen der richtigen Spaltenreihenfolge
			if (row % 2 == 0)
			{ // Gerade Reihe
				// Spalte von rechts nach links
				buffer[i] = Gamma_LUT_Init(row, (15 - col), index); // Umgekehrte Reihenfolge fï¿½r gerade Reihen
			}
			else
			{ // Ungerade Reihe
				// Spalte von links nach rechts
				buffer[i] = Gamma_LUT_Init(row, col, index); // Von links nach rechts fï¿½r ungerade Reihen
			}
			i++;
		}
	}
	return i;
}

void display_matrix_dma0(uint32_t *buffer, int channel)
{
	int i = 0;

	// Farben von led_matrix in den Buffer konvertieren

	i = process_matrix_section(buffer, i, 0, 16, 0, 16, 0);
	i = process_matrix_section(buffer, i, 0, 16, 16, 32, 0);
	i = process_matrix_section(buffer, i, 16, 32, 16, 32, 0);
	i = process_matrix_section(buffer, i, 16, 32, 0, 16, 0);

	// DMA-ï¿½bertragung starten
	Xil_DCacheFlush();
}

void display_matrix_dma1(uint32_t *buffer, int channel)
{
	int i = 0;

	// Farben von led_matrix in den Buffer konvertieren

	i = process_matrix_section(buffer, i, 0, 16, 32, 48, 1);
	i = process_matrix_section(buffer, i, 0, 16, 48, 64, 1);
	i = process_matrix_section(buffer, i, 16, 32, 48, 64, 1);
	i = process_matrix_section(buffer, i, 16, 32, 32, 48, 1);

	// DMA-ï¿½bertragung starten
	Xil_DCacheFlush();
}

void display_matrix_dma2(uint32_t *buffer, int channel)
{
	int i = 0;

	// Farben von led_matrix in den Buffer konvertieren

	i = process_matrix_section(buffer, i, 32, 48, 32, 48, 2);
	i = process_matrix_section(buffer, i, 32, 48, 48, 64, 2);
	i = process_matrix_section(buffer, i, 48, 64, 48, 64, 2);
	i = process_matrix_section(buffer, i, 48, 64, 32, 48, 2);

	// DMA-ï¿½bertragung starten
	//Xil_DCacheFlush();
	Xil_DCacheFlushRange((UINTPTR)buffer, BUFFER_SIZE * sizeof(uint32_t));
}

void display_matrix_dma3(uint32_t *buffer, int channel)
{
	int i = 0;

	// Farben von led_matrix in den Buffer konvertieren

	i = process_matrix_section(buffer, i, 32, 48, 0, 16, 3);
	i = process_matrix_section(buffer, i, 32, 48, 16, 32, 3);
	i = process_matrix_section(buffer, i, 48, 64, 16, 32, 3);
	i = process_matrix_section(buffer, i, 48, 64, 0, 16, 3);

	// DMA-ï¿½bertragung starten
	Xil_DCacheFlush();
}

/************************** Interrupt Controller Initialization ******************************/
// Function to initialize the Interrupt Controller
int Init_Interrupt_Controller()
{
	int Status;
	intc_cfg_ptr = XScuGic_LookupConfig(XPAR_PS7_SCUGIC_0_DEVICE_ID);
	if (NULL == intc_cfg_ptr)
	{
		return XST_FAILURE;
	}
	Status = XScuGic_CfgInitialize(&intc, intc_cfg_ptr, intc_cfg_ptr->CpuBaseAddress);
	if (Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

/************************** Connect DMA Interrupts ******************************/
// Function to connect the interrupts
// map interrupt to cpu is done in XScuGic_Enable()
void Connect_DMA_Interrupts()
{
	// Connect interrupt handlers for each DMA channel
	XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR, (Xil_ExceptionHandler)DMA_Interrupt_Handler_Ch1, &DMA_CH1_inst);
	XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_1_MM2S_INTROUT_INTR, (Xil_ExceptionHandler)DMA_Interrupt_Handler_Ch2, &DMA_CH2_inst);
	XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_2_MM2S_INTROUT_INTR, (Xil_ExceptionHandler)DMA_Interrupt_Handler_Ch3, &DMA_CH3_inst);
	XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_3_MM2S_INTROUT_INTR, (Xil_ExceptionHandler)DMA_Interrupt_Handler_Ch4, &DMA_CH4_inst);

	// set priority and trigger typedef 0x3 rising edge 0x1 high
	XScuGic_SetPriorityTriggerType(&intc, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR, 0x03, 0x3);
	// XScuGic_InterruptMaptoCpu(&intc, (u8)XScuGic_GetCPUID(), XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);

	// Enable interrupts for each DMA channel
	XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);
	XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_1_MM2S_INTROUT_INTR);
	XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_2_MM2S_INTROUT_INTR);
	XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_3_MM2S_INTROUT_INTR);
}

/************************** Enable Interrupts ******************************/
// Function to enable the interrupts
void Enable_Interrupts()
{
	Xil_ExceptionInit(); // does nothing
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &intc);
	Xil_ExceptionEnable(); // calls Xil_ExceptionEnableMask(XIL_EXCEPTION_ID_IRQ_INT);
}







// Function to convert from xy coordinate to serpentine
int XYtoSerpentine(int row, int col) {
    int serpentine = col;
	// when row is odd then
	if (row % 2 == 1){
		// inverse column number
		serpentine = 15 - col;
	}
	// calculate serpentine coordinate
    return serpentine + row * 16;
}



// Function to update a single channel in the buffer
void Update_Channel4(uint32_t* buffer, int channel, int time, uint32_t color) {
    double frequency = 0.2;  // Adjust the frequency of the animation
    double phase = channel * 2.0 * M_PI / NUM_CHANNELS;

	//calculate some blinky blinky
    for (int col = 0; col < 64; col++) {
        for (int row = 0; row < 16; row++) {
            int i = XYtoSerpentine(col, row);

			//get values between 0 and 1
            //float val1 = sin(frequency * i + phase + 0.1 * time) * 0.5 + 0.5;
            float value = 1-fabs(sin(frequency * row + phase + 0.1 * time));

			uint32_t b = (uint32_t)(((color >> 0) & 0xFF) * value);
			uint32_t r = (uint32_t)(((color >> 8) & 0xFF) * value);
			uint32_t g = (uint32_t)(((color >> 16) & 0xFF) * value);

			// gamma correction
			b = gamma_lut[b];
			r = gamma_lut[r];
			g = gamma_lut[g];

			// update the colors in the buffer for the current LED
			buffer[i] = (buffer[i] & 0xFF000000) | // Preserve upper 8 bits
						(b << 16)                | // Blue: bits 16-23
						(r << 8)                 | // Red: bits 8-15
						(g << 0); 		           // Green: bits 0-7
        }
    }
	//needed to force DMA to work
    //Xil_DCacheFlush();
    Xil_DCacheFlushRange((UINTPTR)buffer, BUFFER_SIZE * sizeof(uint32_t));
}







/*
---------------------- MAIN ----------------------
*/

int main()
{




/*
	ip_addr_t ipaddr, netmask, gw;

	unsigned char mac_ethernet_address[] =
		{0x00, 0x0a, 0x35, 0x00, 0x01, 0x02};

	echo_netif = &server_netif;
#if defined(__arm__) && !defined(ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
	ProgramSi5324();
	ProgramSfpPhy();
#endif
#endif


#ifdef XPS_BOARD_ZCU102
	if (IicPhyReset())
	{
		xil_printf("Error performing PHY reset \n\r");
		return -1;
	}
#endif

	init_platform();

#if LWIP_IPV6 == 0
#if LWIP_DHCP == 1
	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;
#else
	// initialize IP addresses to be used
	IP4_ADDR(&ipaddr, 192, 168, 1, 10);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	IP4_ADDR(&gw, 192, 168, 1, 1);
#endif
#endif
	print_app_header();

	lwip_init();

#if (LWIP_IPV6 == 0)
	//Add network interface to the netif_list, and set it as default
	if (!xemac_add(echo_netif, &ipaddr, &netmask,
				   &gw, mac_ethernet_address,
				   PLATFORM_EMAC_BASEADDR))
	{
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
#else
	// Add network interface to the netif_list, and set it as default
	if (!xemac_add(echo_netif, NULL, NULL, NULL, mac_ethernet_address,
				   PLATFORM_EMAC_BASEADDR))
	{
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
	echo_netif->ip6_autoconfig_enabled = 1;

	netif_create_ip6_linklocal_address(echo_netif, 1);
	netif_ip6_addr_set_state(echo_netif, 0, IP6_ADDR_VALID);

	print_ip6("\n\rBoard IPv6 address ", &echo_netif->ip6_addr[0].u_addr.ip6);

#endif
	netif_set_default(echo_netif);

	// now enable interrupts
	platform_enable_interrupts();

	// specify that the network if is up
	netif_set_up(echo_netif);

#if (LWIP_IPV6 == 0)
#if (LWIP_DHCP == 1)
	/
	dhcp_start(echo_netif);
	dhcp_timoutcntr = 24;

	while (((echo_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0))
		xemacif_input(echo_netif);

	if (dhcp_timoutcntr <= 0)
	{
		if ((echo_netif->ip_addr.addr) == 0)
		{
			xil_printf("DHCP Timeout\r\n");
			xil_printf("Configuring default IP of 192.168.1.10\r\n");
			IP4_ADDR(&(echo_netif->ip_addr), 192, 168, 1, 10);
			IP4_ADDR(&(echo_netif->netmask), 255, 255, 255, 0);
			IP4_ADDR(&(echo_netif->gw), 192, 168, 1, 1);
		}
	}

	ipaddr.addr = echo_netif->ip_addr.addr;
	gw.addr = echo_netif->gw.addr;
	netmask.addr = echo_netif->netmask.addr;
#endif

	print_ip_settings(&ipaddr, &netmask, &gw);

#endif
	*/

	// start the application (web server, rxtest, txtest, etc..)
	//start_application();



	 // General Init
	 int counter = 0;

	 for (int i = 0; i < NUM_CHANNELS; i++)
	 {

	 	// start with empty buffer
	 	Clear_Buffer(LED_BUFFER_Instances[i]);

	 	// Reset TxDone Flags

	 }



	DMA_INIT();			 // KEIN PROBLEM
	Xil_DCacheDisable(); // KEIN PROBLEM
					// Initialize the interrupt controller
	if (Init_Interrupt_Controller() != XST_SUCCESS) // AB HIER PROBLEM
					{
						xil_printf("Error initializing interrupt controller\n");
						return XST_FAILURE;
					}

					// Call Interrupt initialization functions, order important
					Enable_Interrupts();
					Connect_DMA_Interrupts();



	/* receive and process packets */
	while (1)
	{
		/*if (TcpFastTmrFlag)
		{
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
		}
		if (TcpSlowTmrFlag)
		{
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
		}*/






			for (int channel = 0; channel < NUM_CHANNELS; channel++)
			{
				// Check if DMA is ready



					if (channel == 0)
					{
						//display_matrix_dma0(LED_BUFFER_Instances[channel], channel);
						Update_Channel4(LED_BUFFER_Instances[channel], channel, counter, 0x005000);
					}
					if (channel == 1)
					{
						//display_matrix_dma1(LED_BUFFER_Instances[channel], channel);
						 Update_Channel4(LED_BUFFER_Instances[channel], channel, counter, 0x500000);
					}
					if (channel == 2)
					{
						//display_matrix_dma2(LED_BUFFER_Instances[channel], channel);
						 Update_Channel4(LED_BUFFER_Instances[channel], channel, counter, 0x000050);
					}
					if (channel == 3)
					{
						//display_matrix_dma3(LED_BUFFER_Instances[channel], channel);
						 Update_Channel4(LED_BUFFER_Instances[channel], channel, counter, 0x005000);
					}



					// Start DMA transfer
					// BUFFER SIZE * 4BYTE per LED
					// XAxiDma_SimpleTransfer(DMA_Instances[channel], (UINTPTR)LED_BUFFER_Instances[channel], BUFFER_SIZE * 4, XAXIDMA_DMA_TO_DEVICE);
					u32 Transmit_Status = XAxiDma_SimpleTransfer(DMA_Instances[channel], (UINTPTR)LED_BUFFER_Instances[channel], 4 * BUFFER_SIZE, XAXIDMA_DMA_TO_DEVICE);

					for (volatile int i = 0; i < 0xFFFFF; i++)
						;

					counter++;


			}//for

		//if Dataflag

		xil_printf("While durch");

		//xemacif_input(echo_netif);
		//transfer_data();
	}//while

	/* never reached */
	cleanup_platform();

	return 0;
}

// Timer f�r lwip wird eventuell deaktiviert

/************************** Generic DMA Interrupt Handler ******************************/
// Use Generic Interrupt Function:
void DMA_Interrupt_Function(XAxiDma *AxiDMAInst, int channel)
{
	// Handle interrupt for channel

	// Get IRQ
	u32 irqStatus = XAxiDma_IntrGetIrq(AxiDMAInst, XAXIDMA_DMA_TO_DEVICE);
	// acknowledge pending IRQ
	XAxiDma_IntrAckIrq(AxiDMAInst, irqStatus, XAXIDMA_DMA_TO_DEVICE);

	// combine with mask
	if (irqStatus & XAXIDMA_IRQ_IOC_MASK)
	{
		// Set the completion flag for the corresponding channel
		TxDoneA[channel] = 1;
	}
}

/************************** Specific Interrupt Handlers ******************************/
// Specific Interrupt Handler
void DMA_Interrupt_Handler_Ch1(void *CallbackRef)
{
	DMA_Interrupt_Function((XAxiDma *)CallbackRef, 0);
}

void DMA_Interrupt_Handler_Ch2(void *CallbackRef)
{
	DMA_Interrupt_Function((XAxiDma *)CallbackRef, 1);
}

void DMA_Interrupt_Handler_Ch3(void *CallbackRef)
{
	DMA_Interrupt_Function((XAxiDma *)CallbackRef, 2);
}

void DMA_Interrupt_Handler_Ch4(void *CallbackRef)
{
	DMA_Interrupt_Function((XAxiDma *)CallbackRef, 3);
}
