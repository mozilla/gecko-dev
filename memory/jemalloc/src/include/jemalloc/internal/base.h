/******************************************************************************/
#ifdef JEMALLOC_H_TYPES

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS

#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

/* base_mtx synchronizes base allocations and protects base_allocated */
extern malloc_mutex_t	base_mtx;
extern size_t base_allocated;

void	*base_alloc(size_t size);
void	*base_calloc(size_t number, size_t size);
extent_node_t *base_node_alloc(void);
void	base_node_dalloc(extent_node_t *node);
bool	base_boot(void);
void	base_prefork(void);
void	base_postfork_parent(void);
void	base_postfork_child(void);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES

#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/
