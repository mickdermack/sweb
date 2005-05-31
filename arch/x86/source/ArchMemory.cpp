//----------------------------------------------------------------------
//  $Id: ArchMemory.cpp,v 1.6 2005/05/25 08:27:48 nomenquis Exp $
//----------------------------------------------------------------------
//
//  $Log: ArchMemory.cpp,v $
//  Revision 1.5  2005/05/20 14:07:20  btittelbach
//  Redesign everything
//
//  Revision 1.4  2005/05/20 11:58:10  btittelbach
//  much much nicer UserProcess Page Management, but still things to do
//
//  Revision 1.3  2005/05/19 20:04:16  btittelbach
//  Much of this still needs to be moved to arch
//
//  Revision 1.2  2005/05/19 19:35:30  btittelbach
//  ein bisschen Arch Memory
//
//  Revision 1.1  2005/05/16 20:37:51  nomenquis
//  added ArchMemory for page table manip
//
//----------------------------------------------------------------------

#include "ArchMemory.h"
#include "kprintf.h"

extern "C" uint32 kernel_page_directory_start;

void ArchMemory::initNewPageDirectory(uint32 physical_page_to_use)
{
  page_directory_entry *new_page_directory = (page_directory_entry*) get3GBAdressOfPPN(physical_page_to_use);
  
  ArchCommon::memcpy((pointer) new_page_directory,(pointer) &kernel_page_directory_start, PAGE_SIZE);
  for (uint32 p = 0; p < 512; ++p) //we're concerned with first two gig, rest stays as is
  {
    new_page_directory[p].pde4k.present=0;
    new_page_directory[p].pde4m.present=0;
  }
}

void ArchMemory::checkAndRemovePTE(uint32 physical_page_directory_page, uint32 pde_vpn)
{
  page_directory_entry *page_directory = (page_directory_entry *) get3GBAdressOfPPN(physical_page_directory_page);
  page_table_entry *pte_base = (page_table_entry *) get3GBAdressOfPPN(page_directory[pde_vpn].pde4k.page_table_base_address);
  for (uint32 pte_vpn=0; pte_vpn < PAGE_TABLE_ENTRIES; ++pte_vpn)
    if (pte_base[pte_vpn].present > 0)
      return; //not empty -> do nothing
    
  //else:
  page_directory[pde_vpn].pde4k.present = 0;
  PageManager::instance()->freePage(page_directory[pde_vpn].pde4k.page_table_base_address);
}

void ArchMemory::unmapPage(uint32 physical_page_directory_page, uint32 virtual_page)
{
  page_directory_entry *page_directory = (page_directory_entry *) get3GBAdressOfPPN(physical_page_directory_page);
  uint32 pde_vpn = virtual_page / PAGE_TABLE_ENTRIES;
  uint32 pte_vpn = virtual_page % PAGE_TABLE_ENTRIES;
  
  page_table_entry *pte_base = (page_table_entry *) get3GBAdressOfPPN(page_directory[pde_vpn].pde4k.page_table_base_address);
  if (pte_base[pte_vpn].present)
  {
    pte_base[pte_vpn].present = 0;
    PageManager::instance()->freePage(pte_base[pte_vpn].page_base_address);
  }
  checkAndRemovePTE(physical_page_directory_page, pde_vpn);
}

void ArchMemory::insertPTE(uint32 physical_page_directory_page, uint32 pde_vpn, uint32 physical_page_table_page)
{
  page_directory_entry *page_directory = (page_directory_entry *) get3GBAdressOfPPN(physical_page_directory_page);
  ArchCommon::bzero(get3GBAdressOfPPN(physical_page_table_page),PAGE_SIZE);
  page_directory[pde_vpn].pde4k.present = 1;
	page_directory[pde_vpn].pde4k.writeable = 1;
  page_directory[pde_vpn].pde4k.page_table_base_address = physical_page_table_page; 
	page_directory[pde_vpn].pde4k.user_access = 1;
}
pointer ArchMemory::physicalPageToKernelPointer(uint32 physical_page)
{
  return physical_page * PAGE_SIZE + 1024*1024*1024*3;
}

void ArchMemory::mapPage(uint32 physical_page_directory_page, uint32 virtual_page, uint32 physical_page, uint32 user_access)
{
  kprintf("pys1 %x, pyhs2 %x",physical_page_directory_page, physical_page);
  page_directory_entry *page_directory = (page_directory_entry *) get3GBAdressOfPPN(physical_page_directory_page);
  uint32 pde_vpn = virtual_page / PAGE_TABLE_ENTRIES;
  uint32 pte_vpn = virtual_page % PAGE_TABLE_ENTRIES;
  if (page_directory[pde_vpn].pde4k.present == 0)
  {
    kprintf("Need to add a pte for 0 %d %d %d\n",pde_vpn, virtual_page, physical_page);
    insertPTE(physical_page_directory_page,pde_vpn,PageManager::instance()->getFreePhysicalPage());
  }
  page_table_entry *pte_base = (page_table_entry *) get3GBAdressOfPPN(page_directory[pde_vpn].pde4k.page_table_base_address);
  kprintf("pte_base = %x\n",pte_base);
  pte_base[pte_vpn].present = 1;
  pte_base[pte_vpn].writeable = 1;
  pte_base[pte_vpn].user_access = user_access;
 // pte_base[pte_vpn].accessed = 0;
 // pte_base[pte_vpn].dirty = 0;
  pte_base[pte_vpn].page_base_address = physical_page;
}

void ArchMemory::freePageDirectory(uint32 physical_page_directory_page)
{
  page_directory_entry *page_directory = (page_directory_entry *) get3GBAdressOfPPN(physical_page_directory_page);
  for (uint32 pde_vpn=0; pde_vpn < PAGE_TABLE_ENTRIES; ++pde_vpn)
  {
    if (page_directory[pde_vpn].pde4k.present)
    {
      page_table_entry *pte_base = (page_table_entry *) get3GBAdressOfPPN(page_directory[pde_vpn].pde4k.page_table_base_address);
      for (uint32 pte_vpn=0; pte_vpn < PAGE_TABLE_ENTRIES; ++pte_vpn)
      {
        if (pte_base[pte_vpn].present)
        {
          pte_base[pte_vpn].present = 0;
          PageManager::instance()->freePage(pte_base[pte_vpn].page_base_address);
        }
      }
      page_directory[pde_vpn].pde4k.present=0;
      PageManager::instance()->freePage(page_directory[pde_vpn].pde4k.page_table_base_address);
    }
  }
  PageManager::instance()->freePage(physical_page_directory_page);
}