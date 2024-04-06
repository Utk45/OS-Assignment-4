#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "x86.h"
#include "proc.h"
#include "mmu.h"



struct swap_slot slotsList[NSLOTS];
void
initswaplist(){
  for(int i = 0; i<NSLOTS; i++){
    slotsList[i].is_free = 1;
  }
}


struct vicpage
findvictimpage(){
    // if there is a page that is not accessed, return that page
    // if all pages are accessed, convert 10% of the pages to not accessed

    int found = 0;
    struct proc *p = findvictimproc();
    pde_t *pde = p->pgdir;
    int count = 0;
    struct vicpage vp;
    for(int i = 0; i < NPTENTRIES; i++){
        if(pde[i] & PTE_P){
            if(!(pde[i] & PTE_A)){
                p->rss--;
                vp.page = (char*)P2V(PTE_ADDR(pde[i]));
                vp.pte = &pde[i];
                return vp;
            }else{
                count++;
            }
        }
    }
    int num_of_pages_to_convert = (count+9)/10;
    for(int i = 0; i < NPDENTRIES; i++){
        if(pde[i] & PTE_P){
            pde[i] = pde[i] ^ PTE_A;
            num_of_pages_to_convert--;
            if(num_of_pages_to_convert == 0){
                p->rss--;
                vp.page = (char*)P2V(PTE_ADDR(pde[i]));
                vp.pte = &pde[i];
                return vp;
            }
        }
    }
}

uint
getFreeSwapSlot(){
    for(uint i = 0; i<NSLOTS; i++){
        if(slotsList[i].is_free){
            return 2 + i*SLOT_SIZE;
        }
    }
    return 0;
}

char*
swapout(){
    struct vicpage vp = findvictimpage();
    uint slot = getFreeSwapSlot();
    if(slot == 0){
        panic("No free swap slot available");
    }
    for (int i = 0; i < SLOT_SIZE; i++){
        wsect(slot+i, (void*)(vp.page+i*BSIZE));
    }
    *vp.pte = PTE_FLAGS(*vp.pte) | (slot<<12);
    *vp.pte = *vp.pte ^ PTE_P;
    *vp.pte = *vp.pte ^ PTE_S;
    slotsList[(slot-2)/8].is_free = 0;
    slotsList[(slot-2)/8].page_perm = PTE_FLAGS(*vp.pte);
    return vp.page;
}

char *
swapin(uint slot){
    char* page = kalloc();
    if(page == 0){
        panic("swapin: kalloc failed");
    }
    for(int i = 0; i < SLOT_SIZE; i++){
        rsect(slot+i, (void*)(page+i*BSIZE));
    }
    // slotsList[(slot-2)/8].is_free = 1;
    // slotsList[(slot-2)/8].page_perm;
    return page;
}

void
freeSwapSlot(uint slot){
    slotsList[(slot-2)/8].is_free = 1;
}

void 
cleanSwap(pde_t* pde){
    for(int i = 0; i < NPTENTRIES; i++){
        if(pde[i] & PTE_S){
            uint slot = PTE_ADDR(pde[i]) >> 12;
            freeSwapSlot(slot);
        }
    }
}

void 
pagefault_handler(){
    uint va = rcr2();
    struct proc *p = myproc();
    pte_t* pte = walkpgdir(p->pgdir, (void*)va, 0);
    if(pte == 0){
        panic("pagefault_handler: pte is null");
    }
    if(*pte & PTE_P){
        panic("pagefault_handler: page is already present");
    }
    uint slot = *pte >> 12;
    char* page = swapin(slot);
    uint permissions = slotsList[(slot-2)/8].page_perm;
    slotsList[(slot-2)/8].is_free = 1;
    *pte = PTE_ADDR(V2P(page)) | PTE_FLAGS(permissions);
    *pte = *pte ^ PTE_P;
    *pte = *pte ^ PTE_S;
}