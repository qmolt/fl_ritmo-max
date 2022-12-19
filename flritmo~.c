#include "flritmo~.h"

void ext_main(void *r)
{
	t_class *c = class_new("flritmo~", (method)fl_ritmo_new, (method)fl_ritmo_free, sizeof(t_fl_ritmo), 0, A_GIMME, 0);

	class_addmethod(c, (method)fl_ritmo_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)fl_ritmo_float, "float", A_FLOAT, 0);
	class_addmethod(c, (method)fl_ritmo_int, "int", A_LONG, 0);
	class_addmethod(c, (method)fl_ritmo_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)fl_ritmo_bar, "bar", A_GIMME, 0);
	class_addmethod(c, (method)fl_ritmo_loop, "loop", A_GIMME, 0);
	class_addmethod(c, (method)fl_ritmo_bang, "bang", 0);

	class_dspinit(c);
	class_register(CLASS_BOX, c);
	fl_ritmo_class = c;
	
	return;
}

void *fl_ritmo_new(t_symbol *s, short argc, t_atom *argv)
{
	t_fl_ritmo *x = (t_fl_ritmo *)object_alloc(fl_ritmo_class);

	dsp_setup((t_pxobject *)x, 1);
	x->m_outlet2 = outlet_new((t_object *)x, "bang"); //bang
	x->m_outlet1 = outlet_new((t_object *)x, "float"); //dur
	x->obj.z_misc |= Z_NO_INPLACE;

	x->total_old_unos = 0;
	x->total_new_unos = 0;
	x->old_cifra = 0.;
	x->new_cifra = 0.;
	x->index_old_unos = 0;
	x->index_new_unos = 0;

	x->samp_count = 0;

	x->beat_ms = DFLT_BEATMS;

	x->old_list_busy = 0;
	x->onoff = 0;
	x->loop = 0;

	x->fs = sys_getsr();

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
	x->m_clock2 = clock_new((t_object *)x, (method)fl_ritmo_finalbang);

	return x;
}

void fl_ritmo_assist(t_fl_ritmo *x, void *b, long msg, long arg, char *dst)
{
	if (msg == ASSIST_INLET) {
		switch (arg) {
		case I_MSBEAT: sprintf(dst, "(int) on/off; (list) bar; (sig~) beat period in milliseconds"); break;
		}
	}
	else if (msg == ASSIST_OUTLET) {
		switch (arg) {
		case O_OUTPUT: sprintf(dst, "(int) note duration"); break;
		case O_FINALFLAG: sprintf(dst, "(bang) end flag"); break;
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

	if (n > 0) {
		x->samp_count = 0;
		x->index_old_unos = 0;
		x->index_new_unos = 0;
		x->onoff = 1;
	}
	else { x->onoff = 0; }
}

void fl_ritmo_loop(t_fl_ritmo *x, t_symbol *msg, short argc, t_atom *argv)
{ 
	t_atom *ap = argv;
	long ac = argc;
	short loop;

	if (ac != 1) { object_error((t_object *)x, "loop: only 1 argument"); return; }
	if (atom_gettype(ap) != A_LONG) { object_error((t_object *)x, "loop: argument must be an integer (0/1)"); return; }
	
	loop = (short)atom_getlong(ap);
	x->loop = loop ? 1 : 0;
}

void fl_ritmo_bar(t_fl_ritmo *x, t_symbol *msg, short argc, t_atom *argv)
{
	//formato: bar f(ó i) <XXXX (X={0,1,-}) 
	t_atom *ap = argv;
	long ac = argc;

	int index_string;
	float acum_beat;
	float beat;
	long acum_unos;
	long subdiv;
	long acum_notes;
	long total_notes;
	long n_groups;
	char *string_bar;

	long k = 0;
	long legatura = 0;

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

	n_groups = ac / 2;
	acum_beat = 0.;
	acum_notes = 0;
	for (long i = 0; i < n_groups; i++) {
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

		if (acum_unos > 0) {
			for (long j = 1; j < subdiv + 1; j++) {
				if (string_bar[j] == '1' && acum_notes < MAX_UNOS_SIZE) {
					
					k = 1;
					while (string_bar[j + k] == '-') { k++; }
					legatura = k;

					x->new_unos[acum_notes].dur_beat = (beat * legatura) / subdiv;
					x->new_unos[acum_notes].inicio_beat = acum_beat + beat * (j-1) / subdiv;
					acum_notes++;
				}
			}
		}

		acum_beat += beat;
	}
	x->new_cifra = acum_beat;
	x->total_new_unos = total_notes = acum_notes;

	x->old_list_busy = 1;
	for (long i = 0; i < total_notes; i++) {
		x->old_unos[i].dur_beat = x->new_unos[i].dur_beat;
		x->old_unos[i].inicio_beat = x->new_unos[i].inicio_beat;
	}
	x->old_cifra = x->new_cifra;
	x->total_old_unos = x->total_new_unos;
	x->old_list_busy = 0;
}

void fl_ritmo_out(t_fl_ritmo *x)
{
	float note_dur;

	if (x->old_list_busy) { 
		note_dur = x->new_unos[x->index_out].dur_beat * x->beat_ms;
	}
	else {
		note_dur = x->old_unos[x->index_out].dur_beat * x->beat_ms;
	}
	outlet_float(x->m_outlet1, note_dur);
}

void fl_ritmo_finalbang(t_fl_ritmo *x) 
{
	outlet_bang(x->m_outlet2);
}

void fl_ritmo_free(t_fl_ritmo *x)
{
	dsp_free((t_pxobject *)x);

	sysmem_freeptr(x->old_unos);
	sysmem_freeptr(x->new_unos);
}

void fl_ritmo_dsp64(t_fl_ritmo *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	x->beatmsin_connected = count[0];

	if (x->fs != samplerate) {
		x->samp_count = (long)(x->samp_count / x->fs * samplerate);

		x->fs = samplerate;
	}
	
	object_method(dsp64, gensym("dsp_add64"), x, fl_ritmo_perform64, 0, NULL);
}

void fl_ritmo_perform64(t_fl_ritmo *x, t_object *dsp64, double **inputs, long numinputs, double **outputs, long numoutputs,
	long vectorsize, long flags, void *userparams)
{
	t_double *beatmsin = inputs[0];

	long n = vectorsize;
	double fs = x->fs;

	fl_beat *p_unos = x->old_unos;

	short onoff = x->onoff;
	short loop = x->loop;
	long samp_count = x->samp_count;
	long samp_note = 0;

	long index_unos = x->index_old_unos;
	long total_unos = x->total_old_unos;

	long beat_samps;
	long bar_samps;
	float cifra = x->old_cifra;

	float beat_ms = 0.;

	if (x->old_list_busy) {
		p_unos = x->new_unos;
		cifra = x->new_cifra;
		index_unos = x->index_new_unos;
		total_unos = x->total_new_unos;
	}

	while (n--){
		
		if (x->beatmsin_connected) { 
			beat_ms = (float)*beatmsin++;
			beat_ms = MAX(MIN_BEATMS, beat_ms);
		}
		else { beat_ms = DFLT_BEATMS; }

		beat_samps = (long)(beat_ms * fs * 0.001);
		bar_samps = (long)(cifra * (float)beat_samps);	
		
		if(onoff){
			if (index_unos < total_unos) {
				samp_note = (long)(p_unos[index_unos].inicio_beat * (float)beat_samps);
				if (samp_count > samp_note) {
					x->index_out = index_unos;
					x->beat_ms = beat_ms;
					clock_delay(x->m_clock, 0);
					index_unos++;
				}
			}
			else {
				if (samp_count > bar_samps) {
					if (!loop) { onoff = 0; }
					samp_count = 0;
					index_unos = 0;
					index_unos = 0;
					clock_delay(x->m_clock2, 0);
				}
			}
			samp_count++;
		}
	}

	x->onoff = onoff;
	x->index_new_unos = index_unos;
	x->index_old_unos = index_unos;
	x->samp_count = samp_count;
}