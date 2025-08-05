#include <kos.h>
#include "primitive.h"
#include "dms.h"


static int current_type = 0;
static pvr_dr_state_t dr_state;
static int dr_initialized = 0;


int primitive_header(void *header, int size, pvr_dr_state_t *dr_state_ptr)
{
    unsigned int *s = (unsigned int *)header;

    /* Get list type */
    current_type = (s[0] >> 24) & 0x07;

    // Use the passed DR state pointer directly
    pvr_poly_hdr_t *target = (pvr_poly_hdr_t *)pvr_dr_target(*dr_state_ptr);
    
    // Set the header fields
    target->cmd = ((pvr_poly_hdr_t *)s)->cmd;
    target->mode1 = ((pvr_poly_hdr_t *)s)->mode1;
    target->mode2 = ((pvr_poly_hdr_t *)s)->mode2;
    target->mode3 = ((pvr_poly_hdr_t *)s)->mode3;
    
    // Commit the header
    pvr_dr_commit(target);
    
    return 0;
}

void prim_commit_poly_vert(polygon_vertex_t *p, int eos)
{
    // Use KOS DR API for direct rendering
    pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(dr_state);
    
    // Set vertex data
    vert->flags = eos ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    vert->x = p->x;
    vert->y = p->y;
    vert->z = p->z;
    vert->u = p->u;
    vert->v = p->v;
    vert->argb = p->base.color;
    vert->oargb = p->offset.color;
    
    // Commit the vertex to the PVR
    pvr_dr_commit(vert);
}

void prim_commit_poly_inter(polygon_vertex_t *p, polygon_vertex_t *q, int eos)
{
    // Get DR target
    pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(dr_state);
    
    // Calculate interpolation
    packed_color_t c;
    float pw = 1.0f / p->z;
    float qw = 1.0f / q->z;
    float inter = (pw - NEAR_Z) / (pw - qw);

    // Set vertex data
    vert->flags = eos ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    vert->z = 1.0f / (pw + (qw - pw) * inter);
    vert->x = (p->x * pw + (q->x * qw - p->x * pw) * inter) * vert->z;
    vert->y = (p->y * pw + (q->y * qw - p->y * pw) * inter) * vert->z;
    vert->u = p->u + (q->u - p->u) * inter;
    vert->v = p->v + (q->v - p->v) * inter;

    // Interpolate colors
    c.argb[0] = p->base.argb[0] + (q->base.argb[0] - p->base.argb[0]) * inter;
    c.argb[1] = p->base.argb[1] + (q->base.argb[1] - p->base.argb[1]) * inter;
    c.argb[2] = p->base.argb[2] + (q->base.argb[2] - p->base.argb[2]) * inter;
    c.argb[3] = p->base.argb[3] + (q->base.argb[3] - p->base.argb[3]) * inter;
    vert->argb = c.color;

    c.argb[0] = p->offset.argb[0] + (q->offset.argb[0] - p->offset.argb[0]) * inter;
    c.argb[1] = p->offset.argb[1] + (q->offset.argb[1] - p->offset.argb[1]) * inter;
    c.argb[2] = p->offset.argb[2] + (q->offset.argb[2] - p->offset.argb[2]) * inter;
    c.argb[3] = p->offset.argb[3] + (q->offset.argb[3] - p->offset.argb[3]) * inter;
    vert->oargb = c.color;
    
    // Commit the vertex
    pvr_dr_commit(vert);
}

int primitive_nclip_polygon(pvr_vertex_t *vertex_list, int *index_list, int index_size)
{
	polygon_vertex_t *p = (polygon_vertex_t *)vertex_list;
	int commit_vertex = 0;

	if (index_size < 3)
	{
		return 0;
	}

	/* Check size */
 
	for (; index_size; index_size -= 3)
	{
		/* First and Second point. */
		if ((1.0f / p[*index_list++].z) >= NEAR_Z)
		{
			if ((1.0f / p[*index_list++].z) >= NEAR_Z)
			{
				if ((1.0f / p[*index_list++].z) >= NEAR_Z)
				{
					/* all inside */
					prim_commit_poly_vert(&p[index_list[-3]], 0);
					prim_commit_poly_inter(&p[index_list[-3]], &p[index_list[-2]], 0);
					prim_commit_poly_vert(&p[index_list[-1]], 0);
					prim_commit_poly_inter(&p[index_list[-2]], &p[index_list[-1]], 0);
					prim_commit_poly_vert(&p[index_list[-1]], 1);
					commit_vertex += 5;
				}
				else
				{
					/* 0 inside, 1 and 2 outside */
					prim_commit_poly_vert(&p[index_list[-3]], 0);
					prim_commit_poly_inter(&p[index_list[-3]], &p[index_list[-2]], 0);
					prim_commit_poly_inter(&p[index_list[-1]], &p[index_list[-3]], 1);
					commit_vertex += 3;
				}
			}
		}
		else
		{
			if ((1.0f / p[*index_list++].z) >= NEAR_Z)
			{
				if ((1.0f / p[*index_list++].z) >= NEAR_Z)
				{
					/* 0 outside, 1 and 2 inside */
					prim_commit_poly_inter(&p[index_list[-3]], &p[index_list[-2]], 0);
					prim_commit_poly_vert(&p[index_list[-2]], 0);
					prim_commit_poly_inter(&p[index_list[-1]], &p[index_list[-3]], 0);
					prim_commit_poly_vert(&p[index_list[-2]], 0);
					prim_commit_poly_vert(&p[index_list[-1]], 1);
					commit_vertex += 5;
				}
				else
				{
					/* 0 outside, 1 inside, 2 outside */
					prim_commit_poly_inter(&p[index_list[-3]], &p[index_list[-2]], 0);
					prim_commit_poly_vert(&p[index_list[-2]], 0);
					prim_commit_poly_inter(&p[index_list[-2]], &p[index_list[-1]], 1);
					commit_vertex += 3;
				}
			}
			else
			{
				if ((1.0f / p[*index_list++].z) >= NEAR_Z)
				{
					/* 0 and 1 outside, 2 inside */
					prim_commit_poly_inter(&p[index_list[-1]], &p[index_list[-3]], 0);
					prim_commit_poly_inter(&p[index_list[-2]], &p[index_list[-1]], 0);
					prim_commit_poly_vert(&p[index_list[-1]], 1);
					commit_vertex += 3;
				}
			}
		}
	}

	return commit_vertex;
}

int primitive_nclip_polygon_strip(pvr_vertex_t *vlist, int *dmsIndices, int dmsCount, pvr_dr_state_t *dr_state_ptr)
{
 

     polygon_vertex_t *p = (polygon_vertex_t *)vlist;
    int commit_vertex = 0;
    int i = 0;

    /*   parse the dmsIndices[] chunk by chunk. */
    while (i < dmsCount)
    {
        uint32_t rawIdx = (uint32_t)dmsIndices[i];
        int isStrip = (rawIdx & 0x80000000) != 0;

        if (isStrip)
        {
            /*--------------------------
              TRIANGLE STRIP CHUNK
            --------------------------*/
            uint32_t sId = (rawIdx >> 24) & 0x7F;  
            int stripLen = 0;

            /* Count how many consecutive indices belong to this strip. */
            while ((i + stripLen) < dmsCount)
            {
                uint32_t r2 = (uint32_t)dmsIndices[i + stripLen];
                int r2Strip = (r2 & 0x80000000) != 0;
                uint32_t r2sId = (r2 >> 24) & 0x7F;
                if (!r2Strip || (r2sId != sId))
                    break;
                stripLen++;
            }

            /* 
               We'll store them in a local "count-based" array:
                 [stripLen, idx1, idx2, ..., idxN]
               Then run the near-plane clipping logic on it.
            */
            if (stripLen >= 2)
            {
                /* small local array for just this strip */
                int tmpSize = stripLen + 1;
                /* prob a issue if largest strip is over this but  unlikely*/
                int stripArray[2048];  

                /* Fill in "count-based" format */
                stripArray[0] = stripLen;
                for (int j = 0; j < stripLen; j++)
                {
                    stripArray[1 + j] = (int)(dmsIndices[i + j] & 0x00FFFFFF);
                }

              
                {
                    /* local copy to parse */
                    int *idx = stripArray;
                    int idxSize = tmpSize;

                    while (idxSize)
                    {
                        int clip = 0;
                        int chunkLen = 0;
                        int sub_i = 0;

                        /* chunkLen = stripArray[0]; then skip it */
                        chunkLen = *idx++;
                        idxSize -= (chunkLen + 1);
                        if (chunkLen < 2)
                        {
                            idx += chunkLen;
                            continue;
                        }

                        /* First and Second point. */
                        if ((1.0f / p[*idx++].z) >= NEAR_Z)
                        {
                            if ((1.0f / p[*idx++].z) >= NEAR_Z)
                            {
                                /* 0, 1 inside */
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], 0);
                                commit_vertex += 2;
                                clip = 3;
                            }
                            else
                            {
                                /* 0 inside, 1 outside */
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                commit_vertex += 2;
                                clip = 1;
                            }
                        }
                        else
                        {
                            if ((1.0f / p[*idx++].z) >= NEAR_Z)
                            {
                                /* 0 outside, 1 inside */
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], 0);
                                commit_vertex += 2;
                                clip = 2;
                            }
                        }

                        /* Third point and more. */
                        for (sub_i = 3; sub_i <= chunkLen; sub_i++)
                        {
                            int eos = 0;
                            if (sub_i == chunkLen)
                                eos = 1; /* end of strip */

                            if ((1.0f / p[*idx++].z) >= NEAR_Z)
                                clip |= (1 << 2);

                            switch (clip)
                            {
                            case 1: /* 0 in, 1 & 2 out */
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 1);
                                commit_vertex++;
                                break;
                            case 2: /* 0 out, 1 in, 2 out */
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], eos);
                                commit_vertex++;
                                break;
                            case 3: /* 0 & 1 in, 2 out */
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 4: /* 0 & 1 out, 2 in */
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                if (!(sub_i & 0x01))
                                {
                                    prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                    commit_vertex++;
                                }
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 5: /* 0 in, 1 out, 2 in */
                                prim_commit_poly_vert(&p[idx[-1]], 0);
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 6: /* 0 out, 1 & 2 in */
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 7: /* all in */
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex++;
                                break;
                            default:
                                break;
                            }

                            clip >>= 1;
                        }
                    }
                }
            }

            i += stripLen;
        }
        else
        {
            /*--------------------------
              TRIANGLE (3 verts)
            --------------------------*/
            if (i + 2 < dmsCount)
            {
                /*  handle  like a "strip" of length=3 
                   in a local array: [3, v1, v2, v3]
                */
                int triArray[4];
                triArray[0] = 3;
                triArray[1] = (int)(dmsIndices[i]   & 0x00FFFFFF);
                triArray[2] = (int)(dmsIndices[i+1] & 0x00FFFFFF);
                triArray[3] = (int)(dmsIndices[i+2] & 0x00FFFFFF);

             
                {
                    int *idx = triArray;
                    int idxSize = 4;

                    while (idxSize)
                    {
                        int clip = 0;
                        int chunkLen = 0;
                        int sub_i = 0;

                        chunkLen = *idx++;
                        idxSize -= (chunkLen + 1);
                        if (chunkLen < 2)
                        {
                            idx += chunkLen;
                            continue;
                        }

                         if ((1.0f / p[*idx++].z) >= NEAR_Z)
                        {
                            if ((1.0f / p[*idx++].z) >= NEAR_Z)
                            {
                                /* 0,1 in */
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], 0);
                                commit_vertex += 2;
                                clip = 3;
                            }
                            else
                            {
                                /* 0 in, 1 out */
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                commit_vertex += 2;
                                clip = 1;
                            }
                        }
                        else
                        {
                            if ((1.0f / p[*idx++].z) >= NEAR_Z)
                            {
                                /* 0 out, 1 in */
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], 0);
                                commit_vertex += 2;
                                clip = 2;
                            }
                        }

                        for (sub_i = 3; sub_i <= chunkLen; sub_i++)
                        {
                            int eos = (sub_i == chunkLen) ? 1 : 0;
                            if ((1.0f / p[*idx++].z) >= NEAR_Z)
                                clip |= (1 << 2);

                            switch (clip)
                            {
                            case 1:
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 1);
                                commit_vertex++;
                                break;
                            case 2:
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], eos);
                                commit_vertex++;
                                break;
                            case 3:
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 4:
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                if (!(sub_i & 0x01))
                                {
                                    prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                    commit_vertex++;
                                }
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 5:
                                prim_commit_poly_vert(&p[idx[-1]], 0);
                                prim_commit_poly_inter(&p[idx[-2]], &p[idx[-1]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 6:
                                prim_commit_poly_inter(&p[idx[-1]], &p[idx[-3]], 0);
                                prim_commit_poly_vert(&p[idx[-2]], 0);
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex += 3;
                                break;
                            case 7:
                                prim_commit_poly_vert(&p[idx[-1]], eos);
                                commit_vertex++;
                                break;
                            default:
                                break;
                            }
                            clip >>= 1;
                        }
                    }
                }

                i += 3;
            }
            else
            {
                i++;
            }
        }
    }

    return commit_vertex;
}


int primitive_polygon(pvr_vertex_t *vertex_list, int *index_list, int index_size)
{
	polygon_vertex_t *p = (polygon_vertex_t *)vertex_list;
	int i;

	if (index_size < 3)
	{
		return 0;
	}

	/* Check size */
 

	for (i = 0; i < index_size; i += 3)
	{
		prim_commit_poly_vert(&p[*index_list++], 0);
		prim_commit_poly_vert(&p[*index_list++], 0);
		prim_commit_poly_vert(&p[*index_list++], 1);
	}

	return index_size;
}

int primitive_polygon_strip(pvr_vertex_t *vertex_list, int *index_list, int index_size, pvr_dr_state_t *dr_state_ptr)
{
    polygon_vertex_t *p = (polygon_vertex_t *)vertex_list;
    int commit_vertex = 0;
    int i = 0;

    /* Check size */
 

    while (i < index_size)
    {
        uint32_t rawIdx = (uint32_t)index_list[i];
        int isStrip = (rawIdx & 0x80000000) != 0;

        if (isStrip)
        {
            /*--------------------------
              TRIANGLE STRIP CHUNK
            --------------------------*/
            uint32_t sId = (rawIdx >> 24) & 0x7F;  
            int stripLen = 0;

            /* Count how many consecutive indices belong to this strip. */
            while ((i + stripLen) < index_size)
            {
                uint32_t r2 = (uint32_t)index_list[i + stripLen];
                int r2Strip = (r2 & 0x80000000) != 0;
                uint32_t r2sId = (r2 >> 24) & 0x7F;
                if (!r2Strip || (r2sId != sId))
                    break;
                stripLen++;
            }

            if (stripLen >= 2)
            {
                /* Process the strip directly without near Z clipping */
                for (int j = 0; j < stripLen; j++)
                {
                    int idx = (int)(index_list[i + j] & 0x00FFFFFF);
                    int eos = (j == stripLen - 1) ? 1 : 0;
                    
                    prim_commit_poly_vert(&p[idx], eos);
                    commit_vertex++;
                }
            }

            i += stripLen;
        }
        else
        {
            /*--------------------------
              TRIANGLE (3 verts)
            --------------------------*/
            if (i + 2 < index_size)
            {
                int idx1 = (int)(index_list[i] & 0x00FFFFFF);
                int idx2 = (int)(index_list[i+1] & 0x00FFFFFF);
                int idx3 = (int)(index_list[i+2] & 0x00FFFFFF);
                
                prim_commit_poly_vert(&p[idx1], 0);
                prim_commit_poly_vert(&p[idx2], 0);
                prim_commit_poly_vert(&p[idx3], 1);
                commit_vertex += 3;
                
                i += 3;
            }
            else
            {
                i++;
            }
        }
    }

    return commit_vertex;
}