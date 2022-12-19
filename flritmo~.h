#ifndef fl_ritmo_h
#define fl_ritmo_h

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include <math.h>

#define MAX_UNOS_SIZE 127
#define MIN_BEATMS 50.0f
#define DFLT_BEATMS 500.0f //120bpm

typedef struct _fl_beat {
	float dur_beat;
	float inicio_beat;
}fl_beat;

typedef struct _fl_ritmo {
	t_pxobject obj;

	fl_beat *old_unos;
	fl_beat *new_unos;
	long total_old_unos;
	long total_new_unos;
	float old_cifra; 
	float new_cifra;
	long index_old_unos;
	long index_new_unos;

	float beat_ms;

	long samp_count;

	short old_list_busy;
	short onoff;
	short loop;

	long index_out;

	double fs;

	short beatmsin_connected;

	void *m_outlet1;
	void *m_outlet2;
	void *m_clock;
	void *m_clock2;

} t_fl_ritmo;

enum INLETS { I_MSBEAT, NUM_INLETS };
enum OUTLETS { O_OUTPUT, O_FINALFLAG, NUM_OUTLETS };

static t_class *fl_ritmo_class;

void *fl_ritmo_new(t_symbol *s, short argc, t_atom *argv);
void fl_ritmo_float(t_fl_ritmo *x, double f);
void fl_ritmo_bang(t_fl_ritmo *x);
void fl_ritmo_int(t_fl_ritmo *x, long n);
void fl_ritmo_assist(t_fl_ritmo *x, void *b, long msg, long arg, char *dst);
void fl_ritmo_bar(t_fl_ritmo *x, t_symbol *msg, short argc, t_atom *argv);

void fl_ritmo_out(t_fl_ritmo *x);
void fl_ritmo_finalbang(t_fl_ritmo *x);
void fl_ritmo_bar(t_fl_ritmo *x, t_symbol *msg, short argc, t_atom *argv);
void fl_ritmo_loop(t_fl_ritmo *x, t_symbol *msg, short argc, t_atom *argv);

void fl_ritmo_free(t_fl_ritmo *x);

void fl_ritmo_dsp64(t_fl_ritmo *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void fl_ritmo_perform64(t_fl_ritmo *x, t_object *dsp64, double **inputs, long numinputs, double **outputs, long numoutputs, long vectorsize, long flags, void *userparams);

#endif
