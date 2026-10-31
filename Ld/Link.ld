ENTRY( _start )__stack_size = 2048;PROVIDE( _stack_size = __stack_size );MEMORY{/* CH32V30x_D8C - CH32V305RB-CH32V305FB   CH32V30x_D8 - CH32V303CB-CH32V303RB*//*	FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 128K	RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 32K*/    /* CH32V30x_D8C - CH32V307VC-CH32V307WC-CH32V307RC-CH32V305CC   CH32V30x_D8 - CH32V303VC-CH32V303RC   FLASH + RAM supports the following configuration   For specific choices, please refer :CH32FV2x_V3xRM.PDF\Table 32-3   FLASH-192K + RAM-128K   FLASH-224K + RAM-96K   FLASH-256K + RAM-64K     FLASH-288K + RAM-32K     FLASH-128K + RAM-192K  */	FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 112K /* 112K = 128K - 16K：尾部 16KB（0x0801C000~0x0801FFFF）留给
                  原厂 bootloader（从 CH32V307 提取、已在 V305 上验证）。
                  这个数字必须 >= bootloader 的实际大小，改 bootloader 时要一起改。
                  FU 参数存储区不再固定在这里，而是由下面的 .fustore 段紧跟固件
                  （4KB，全 0xFF），见 fu_store.h | CH32V305GBU6：Flash 128K/SRAM 32K。原值 288K 是 320K 总量器件
                 （CH32V307VC/WC/RC、CH32V305CC）的可重配档位，会让超容镜像
                 也能链接通过，问题拖到下载/运行才暴露 */	RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 32752 /* Top 16B: reset cookie; stack stays 16B aligned. */}PROVIDE(__boot_cookie = 0x20007FFC);SECTIONS{	.init :	{		_sinit = .;		. = ALIGN(4);		KEEP(*(SORT_NONE(.init)))		. = ALIGN(4);		_einit = .;	} >FLASH AT>FLASH  .vector :  {      *(.vector);	  . = ALIGN(64);  } >FLASH AT>FLASH	.text :	{		. = ALIGN(4);		*(.text)		*(.text.*)		*(.rodata)		*(.rodata*)		*(.gnu.linkonce.t.*)		. = ALIGN(4);	} >FLASH AT>FLASH 	.fini :	{		KEEP(*(SORT_NONE(.fini)))		. = ALIGN(4);	} >FLASH AT>FLASH	PROVIDE( _etext = . );	PROVIDE( _eitcm = . );

	/* ==================================================================
	 * FU 参数存储区（音量/静音掉电保存）：4KB = 一个写保护/擦除扇区
	 *   · 位置：紧跟在代码段之后（在 .data 初值之前），由链接器分配；
	 *   · 必须 4KB 对齐：CH32 标准擦除粒度是 4KB 扇区（FLASH_ErasePage），
	 *     只有对齐了「4KB 日志 == 一个扇区」，整片擦除才擦得干净；
	 *   · 放在这里（而不是 .data 之后）：此时链接器的“当前区域”还是 FLASH，
	 *     ALIGN(4096) 才作用在 flash 地址上；放在 .data 之后会被算到 RAM 段。
	 * ================================================================== */
	.fustore ALIGN(4096) :
	{
		PROVIDE( _fu_store_start = . );
		KEEP(*(.fustore))
		. = ALIGN(4096);
		PROVIDE( _fu_store_end = . );
	} >FLASH AT>FLASH
		.preinit_array  :	{	  PROVIDE_HIDDEN (__preinit_array_start = .);	  KEEP (*(.preinit_array))	  PROVIDE_HIDDEN (__preinit_array_end = .);	} >FLASH AT>FLASH 		.init_array     :	{	  PROVIDE_HIDDEN (__init_array_start = .);	  KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))	  KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))	  PROVIDE_HIDDEN (__init_array_end = .);	} >FLASH AT>FLASH 		.fini_array     :	{	  PROVIDE_HIDDEN (__fini_array_start = .);	  KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))	  KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))	  PROVIDE_HIDDEN (__fini_array_end = .);	} >FLASH AT>FLASH 		.ctors          :	{	  /* gcc uses crtbegin.o to find the start of	     the constructors, so we make sure it is	     first.  Because this is a wildcard, it	     doesn't matter if the user does not	     actually link against crtbegin.o; the	     linker won't look for a file to match a	     wildcard.  The wildcard also means that it	     doesn't matter which directory crtbegin.o	     is in.  */	  KEEP (*crtbegin.o(.ctors))	  KEEP (*crtbegin?.o(.ctors))	  /* We don't want to include the .ctor section from	     the crtend.o file until after the sorted ctors.	     The .ctor section from the crtend file contains the	     end of ctors marker and it must be last */	  KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))	  KEEP (*(SORT(.ctors.*)))	  KEEP (*(.ctors))	} >FLASH AT>FLASH 		.dtors          :	{	  KEEP (*crtbegin.o(.dtors))	  KEEP (*crtbegin?.o(.dtors))	  KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))	  KEEP (*(SORT(.dtors.*)))	  KEEP (*(.dtors))	} >FLASH AT>FLASH 	.dalign :	{		. = ALIGN(4);		PROVIDE(_data_vma = .);	} >RAM AT>FLASH		.dlalign :	{		. = ALIGN(4); 		PROVIDE(_data_lma = .);	} >FLASH AT>FLASH	.data :	{    	*(.gnu.linkonce.r.*)    	*(.data .data.*)    	*(.gnu.linkonce.d.*)		. = ALIGN(8);    	PROVIDE( __global_pointer$ = . + 0x800 );    	*(.sdata .sdata.*)		*(.sdata2.*)    	*(.gnu.linkonce.s.*)    	. = ALIGN(8);    	*(.srodata.cst16)    	*(.srodata.cst8)    	*(.srodata.cst4)    	*(.srodata.cst2)    	*(.srodata .srodata.*)    	. = ALIGN(4);		PROVIDE( _edata = .);	} >RAM AT>FLASH	/* ==================================================================
	 * FU 参数存储区（音量/静音掉电保存）
	 *   · 位置：紧跟固件镜像之后（.text + .data 初值），**不在最尾部** ——
	 *     尾部 16KB 已经留给原厂 bootloader；
	 *   · 大小：4KB = 16 页 x 256 字节（CH32 快速擦除按 256 字节页，
	 *     写保护按 4KB 扇区划分，整块 4KB 最省事）；
	 *   · 内容：由 User/src/fu_store.c 里的 const 数组填满 0xFF，等价于
	 *     "向编译器申请一段 0xFF"，下载器会把这段留在已擦除态；
	 *   · 符号：_fu_store_start / _fu_store_end，C 侧只用符号取地址，
	 *     不再硬编码 0x0801F000。
	 * 注意：固件一改大小，这块的地址就跟着移动，旧参数自然失效（回落默认值）。
	 * ================================================================== */

	.bss :	{		. = ALIGN(4);		PROVIDE( _sbss = .);  	    *(.sbss*)        *(.gnu.linkonce.sb.*)		*(.bss*)     	*(.gnu.linkonce.b.*)				*(COMMON*)		. = ALIGN(4);		PROVIDE( _ebss = .);	} >RAM AT>FLASH	PROVIDE( _end = _ebss);	PROVIDE( end = . );    .stack ORIGIN(RAM) + LENGTH(RAM) - __stack_size :    {        PROVIDE( _heap_end = . );            . = ALIGN(4);        PROVIDE(_susrstack = . );        . = . + __stack_size;        PROVIDE( _eusrstack = .);    } >RAM }