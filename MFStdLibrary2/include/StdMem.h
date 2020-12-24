// StdMem.h
// Guy Simmons, 18th December 1997

#ifndef	STD_MEM_H
#define	STD_MEM_H

//---------------------------------------------------------------

BOOL	SetupMemory(void);
void	ResetMemory(void);
void	*MemAlloc(ULONG size);
void	*MemReAlloc(void *ptr, ULONG size);
void	MemFree(void *mem_ptr);



#ifdef _DEBUG
void MFnewTrace ( void *pvAddr, size_t size );
void MFdeleteTrace ( void *pvAddr );
#endif


// Some templated new and delete stand-ins.
template <class T> T *MFnew ( void )
{
	T *ptr = new T;
#ifdef _DEBUG
	MFnewTrace ( ptr, sizeof ( ptr ) );
#endif
	return ptr;
}

template<class T> void MFdelete(T *thing)
{
#ifdef _DEBUG
	MFdeleteTrace ( thing );
#endif
	delete thing;
}

//---------------------------------------------------------------

#endif
