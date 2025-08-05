#ifndef _PRIM_H_INCLUDED_
#define _PRIM_H_INCLUDED_


typedef union {
    unsigned int color;
    unsigned char argb[4];
    float intensity;
} packed_color_t;

typedef struct {
    float x, y, z;
} vertex_3f_t;

typedef struct {
    unsigned int cmd;
    unsigned int mode1;
    unsigned int mode2;
    unsigned int mode3;
    unsigned int color[4];
} polygon_header_t;



typedef struct {
    unsigned int flags;
    float x, y ,z;
    float u, v;
    packed_color_t base;
    packed_color_t offset;
} polygon_vertex_t;



/* Near Z clip value. */
#define NEAR_Z  0.1f

extern int primitive_buffer_init(int type, void *buffer, int size);
extern void primitive_buffer_begin(void);
extern void primitive_buffer_flush(void);

int primitive_header(void *header, int size, pvr_dr_state_t *dr_state_ptr);

extern int primitive_nclip_polygon(pvr_vertex_t *vertex_list, int *index_list, int index_size);
extern int primitive_nclip_polygon_strip(pvr_vertex_t *vlist, int *dmsIndices, int dmsCount, pvr_dr_state_t *dr_state_ptr);


extern int primitive_polygon(pvr_vertex_t *vertex_list, int *index_list, int index_size);
extern int primitive_polygon_strip(pvr_vertex_t *vertex_list, int *index_list, int index_size, pvr_dr_state_t *dr_state_ptr);


extern int prim_commit_vert_ready(int size);
extern void prim_commit_poly_vert(polygon_vertex_t *p, int eos);
extern void prim_commit_poly_inter(polygon_vertex_t *p, polygon_vertex_t *q, int eos);

#endif
