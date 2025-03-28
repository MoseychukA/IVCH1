/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2013        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control module to the FatFs module with a defined API.        */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "sdcard.h"
//#include "usbdisk.h"	/* Example: USB drive control */
//#include "atadrive.h"	/* Example: ATA drive control */
//#include "sdcard.h"		/* Example: MMC/SDC contorl */
//#include "msd.h"
/* Definitions of physical drive number for each media */
#define ATA		0
#define MMC		1
#define USB		2
#define SECTOR_SIZE 512U

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber (0..) */
)
{
  return 0;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber (0..) */
)
{
  return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
  if(count==1)
  {
    SD_ReadBlock(&buff[0] ,sector << 9,SECTOR_SIZE);
    while(SD_GetStatus() != SD_TRANSFER_OK);
  }
  else
  {
    SD_ReadMultiBlocks((&buff[0]), sector << 9,SECTOR_SIZE,count);
    while(SD_GetStatus() != SD_TRANSFER_OK);
  }


  return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
  if(count==1)
  {
    SD_WriteBlock((BYTE*)(&buff[0]),sector << 9, SECTOR_SIZE);
    while(SD_GetStatus() != SD_TRANSFER_OK);
  }
  else
  {
    SD_WriteMultiBlocks((BYTE*)(&buff[0]) , sector << 9, SECTOR_SIZE, count);
    while(SD_GetStatus() != SD_TRANSFER_OK);
  }
        
  return RES_OK;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
  DRESULT res = RES_ERROR;
  SD_CardInfo CardInfo;
  
  switch (cmd)
  {
    case CTRL_SYNC:
      res = RES_OK;
      break;
    
    case GET_SECTOR_COUNT:
      SD_GetCardInfo(&CardInfo);
      *(DWORD*)buff = CardInfo.CardCapacity / 512;
      res = RES_OK;
      break;
      
    case GET_SECTOR_SIZE:
      *(DWORD*)buff = 512;
      res= RES_OK;
      break;
    
    case GET_BLOCK_SIZE:
      *(DWORD*)buff = 512;
      res= RES_OK;
      break;
  }
      
  return res;
}
#endif
