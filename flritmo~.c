#include "flritmo~.h"

void ext_main(void *r)
{
	fl_ritmo_class = class_new("flritmo~", (method)fl_ritmo_new, (method)fl_ritmo_free, sizeof(t_fl_ritmo), 0, A_GIMME, 0);

	class_addmethod(fl_ritmo_class, (method)fl_ritmo_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(fl_ritmo_class, (method)fl_ritmo_float, "float", A_FLOAT, 0);
	class_addmethod(fl_ritmo_class, (method)fl_ritmo_int, "int", A_LONG, 0);
	class_addmethod(fl_ritmo_class, (method)fl_ritmo_assist, "assist", A_CANT, 0);
	class_addmethod(fl_ritmo_class, (method)fl_ritmo_beat_ms, "ft1", A_FLOAT, 0);
	class_addmethod(fl_ritmo_class, (method)fl_ritmo_bar, "bar", A_GIMME, 0);
	class_addmethod(fl_ritmo_class, (method)fl_ritmo_bang, "bang", 0);

	class_dspinit(fl_ritmo_class);

	class_register(CLASS_BOX, fl_ritmo_class);
}

void *fl_ritmo_new(t_symbol *s, short argc, t_atom *argv)
{
	t_fl_ritmo *x = (t_fl_ritmo *)object_alloc(fl_ritmo_class);

	floatin(x, 1);
	dsp_setup((t_pxobject *)x, 0);
	x->m_outlet1 = outlet_new((t_object *)x, NULL);
	x->obj.z_misc |= Z_NO_INPLACE;

	x->total_old_unos = 0;
	x->total_new_unos = 0;
	x->total_old_bar_samps = 0;
	x->total_new_bar_samps = 0;
	x->index_old_unos = 0;
	x->index_new_unos = 0;

	x->samp_count = 0;
	x->beat_ms = 500.0;

	x->new_list_available = -1;
	x->onoff = 0;
	x->loop = 0;

	x->fs = sys_getsr();

	x->isnewlist = 0;
	x->index_out = 0;

	x->old_unos = (fl_beat *)sysmem_newptr(MAX_UNOS_SIZE * sizeof(fl_beat));
	if (!x->old_unos) { object_error((t_object *)x, "out of memory: no space for list"); return x; }
	x->new_unos = (fl_beat *)sysmem_newptr(MAX_UNOS_SIZE * sizeof(fl_beat));
	if (!x->new_unos) { object_error((t_object *)x, "out of memory: no space for list"); return x; }
	for (long i = 0; i < MAX_UNOS_SIZE; i++) {
		x->old_unos[i].dur_beat = 0.0;
		x->old_unos[i].inicio_beat = 0.0;
		x->new_unos[i].dur_beat = 0.0;
		x->new_unos[i].inicio_beat = 0.0;
	}
	
	x->m_clock = clock_new((t_object *)x, (method)fl_ritmo_out);

	return x;
}

void fl_ritmo_assist(t_fl_ritmo *x, void *b, long msg, long arg, char *dst)
{
	if (msg == ASSIST_INLET) {
		switch (arg) {
		case I_BAR: sprintf(dst, "(int) on/off; (bar) list"); break;
		}
	}
	else if (msg == ASSIST_OUTLET) {
		switch (arg) {
		case O_OUTPUT: sprintf(dst, "(int) note duration"); break;
		}
	}
}

void fl_ritmo_float(t_fl_ritmo *x, double f)
{
	fl_ritmo_int(x, (long)f);
}

void fl_ritmo_bang(t_fl_ritmo *x)
{
	fl_ritmo_int(x, 1);
}

void fl_ritmo_int(t_fl_ritmo *x, long n)
{
	if (n != n) { return; }

	if (x->beat_ms > 0 && n > 0) {
		x->samp_count = 0;
		x->index_old_unos = 0;
		x->index_new_unos = 0;
		x->onoff = 1;
	}
	else { x->onoff = 0; }
}

void fl_ritmo_beat_ms(t_fl_ritmo *x, double f)
{
	if (f != f) { return; }
	if (f < 0.0) { return; }

	x->beat_ms = f;
}

void fl_ritmo_bar(t_fl_ritmo *x, t_symbol *msg, short argc, t_atom *argv)
{
	//formato: bar f(ó i) <XXXX (X={0,1}) 
	t_atom *ap = argv;
	long ac = argc;

	int index_string;
	float acum_beat;
	float beat;
	float cifra;
	long acum_unos;
	long unos_subdiv;
	long subdiv;
	long acum_notes;
	long total_notes;
	long n_unos;
	char *string_bar;

	if (ac % 2 == 1) {
		object_error((t_object *)x, "bar: must have even number of elements"); return;
	}
	for (long i = 0; i < ac; i++) {
		if (i % 2 == 0) {
			if (atom_gettype(ap + i) != A_FLOAT && atom_gettype(ap + i) != A_LONG) {
				object_error((t_object *)x, "bar: even args must be numbers"); return;
			}
		}
		else {
			if (atom_gettype(ap + i) != A_SYM) {
				object_error((t_object *)x, "bar: odd args must be strings"); return;
			}
		}
	}

	n_unos = ac / 2;
	acum_beat = 0.;
	acum_notes = 0;
	for (long i = 0; i < n_unos; i++) {
		beat = (float)fabs((double)atom_getfloat(ap + 2 * i));
		string_bar = atom_getsym(ap + 2 * i + 1)->s_name;
		if (string_bar[0] != '<') { object_warn((t_object *)x, "bar: start list with '<'"); continue; }
		index_string = 0;
		acum_unos = 0;
		while (string_bar[index_string] != '\0') {
			if (string_bar[index_string] == '1') {
				acum_unos++;
			}
			index_string++;
		}
		subdiv = index_string - 1;
		unos_subdiv = acum_unos;

		if (unos_subdiv > 0) {
			for (long j = 0; j < subdiv; j++) {
				if (string_bar[j+1] == '1' && acum_notes < MAX_UNOS_SIZE) {
					x->new_unos[acum_notes].dur_beat = beat / subdiv;
					x->new_unos[acum_notes].inicio_beat = acum_beat + beat * j / subdiv;
					acum_notes++;
				}
			}
		}

		acum_beat += beat;
	}
	cifra = acum_beat; //could be an output
	x->total_new_unos = total_notes = acum_notes;

	x->new_list_available = 1;
	for (long i = 0; i < total_notes; i++) {
		x->old_unos[i].dur_beat = x->new_unos[i].dur_beat;
		x->old_unos[i].inicio_beat = x->new_unos[i].inicio_beat;
	}
	x->total_old_unos = x->total_new_unos;
	x->new_list_available = 0;
}

void fl_ritmo_out(t_fl_ritmo *x)
{
	float note_dur;
	if (x->isnewlist) { note_dur = x->new_unos[x->index_out].dur_beat; }
	else { note_dur = x->old_unos[x->index_out].dur_beat; }
	note_dur *= x->beat_ms;
	outlet_float(x->m_outlet1, note_dur);
}

void fl_ritmo_free(t_fl_ritmo *x)
{
	dsp_free((t_pxobject *)x);

	sysmem_freeptr(x->old_unos);
	sysmem_freeptr(x->new_unos);
}

void fl_ritmo_dsp64(t_fl_ritmo *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	if (x->fs != samplerate) {
		x->total_new_bar_samps = (long)(x->total_new_bar_samps / x->fs * samplerate);
		x->total_old_bar_samps = (long)(x->total_old_bar_samps / x->fs * samplerate);
		x->samp_count = (long)(x->samp_count / x->fs * samplerate);

		x->fs = samplerate;
	}
	
	object_method(dsp64, gensym("dsp_add64"), x, fl_ritmo_perform64, 0, NULL);
}

void fl_ritmo_perform64(t_fl_ritmo *x, t_object *dsp64, double **inputs, long numinputs, double **outputs, long numoutputs,
	long vectorsize, long flags, void *userparams)
{
	long n = vectorsize;
	double fs = x->fs;

	fl_beat *pold_unos = x->old_unos;
	fl_beat *pnew_unos = x->new_unos;

	short onoff = x->onoff;
	long samp_count = x->samp_count;
	long samp_note = 0;

	long index_old_unos = x->index_old_unos;
	long index_new_unos = x->index_new_unos;
	long total_old_unos = x->total_old_unos;
	long total_new_unos = x->total_new_unos;
	
	short dirty = x->new_list_available;

	long beat_samps = (long)(x->beat_ms * fs * 0.001);

	while (n--){

		if(onoff){
			if (dirty) {
				if (index_new_unos < total_new_unos) {
					samp_note = (long)(pnew_unos[index_new_unos].inicio_beat * (float)beat_samps);
					if (samp_count > samp_note) {
						x->isnewlist = 1;
						x->index_out = index_new_unos;
						clock_delay(x->m_clock, 0);
						index_new_unos++;
					}
				}
				else {
					onoff = 0;
				}
			}
			else {
				if (index_old_unos < total_old_unos) {
					samp_note = (long)(pold_unos[index_old_unos].inicio_beat * (float)beat_samps);
					if (samp_count > samp_note) {
						x->isnewlist = 0;
						x->index_out = index_old_unos;
						clock_delay(x->m_clock, 0);
						index_old_unos++;
					}
				}
				else {
					onoff = 0;
				}
			}
			samp_count++;
		}
	}

	x->index_new_unos = index_new_unos;
	x->index_old_unos = index_old_unos;
	x->samp_count = samp_count;
}