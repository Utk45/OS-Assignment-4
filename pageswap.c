#include "fs.h"
#include "mkfs.c"
#include "proc.c"

char* findvictimpage();
struct proc *findvictimproc();
struct proc *
findvictimproc(){
    struct proc *p;
    struct cpu *c = mycpu();
    uint max_rss = 0;
    int min_pid = 10000;
    struct proc *victim_proc;
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->rss > max_rss){
            victim_proc = p;
            max_rss = p->rss;
            min_pid = p->pid;
        }else if(p->rss == max_rss && p->pid < min_pid){
            victim_proc = p;
            max_rss = p->rss;
            min_pid = p->pid;
        }
    }
    release(&ptable.lock);
    return victim_proc;
}

char*
findvictimpage(){
    // if there is a page that is not accessed, return that page
    // if all pages are accessed, convert 10% of the pages to not accessed

    int found = 0;
    struct proc *p = findvictimproc();
    pde_t *pde = p->pgdir;
    int count = 0;
    for(int i = 0; i < NPDENTRIES; i++){
        if(pde[i] & PTE_P){
            if(!(pde[i] & PTE_A)){
                pde[i] = pde[i] ^ PTE_P;
                return (char*)P2V(PTE_ADDR(pde[i]));
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
                pde[i] = pde[i] ^ PTE_P;
                return (char*)P2V(PTE_ADDR(pde[i]));
            }
        }
    }
}

uint
getFreeSwapSlot(){
    for(uint i = 0; i<NSLOTS; i++){
        if(slotsList[i].is_free){
            return sb.nswapstart + i*SLOT_SIZE;
        }
    }
    return 0;
}


