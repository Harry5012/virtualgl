/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005, 2006 Sun Microsystems, Inc.
 * Copyright (C)2009-2011 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#include "vgltransconn.h"
#include "rrtimer.h"
#include "fakerconfig.h"
#include "rrutil.h"
#ifdef _WIN32
#include <io.h>
#define _POSIX_
#endif
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#define open _open
#define write _write
#define close _close
#define S_IREAD 0400
#define S_IWRITE 0200
#endif


void vgltransconn::sendheader(rrframeheader h, bool eof=false)
{
	if(_dosend)
	{
		if(_v.major==0 && _v.minor==0)
		{
			// Fake up an old (protocol v1.0) EOF packet and see if the client sends
			// back a CTS signal.  If so, it needs protocol 1.0
			rrframeheader_v1 h1;  char reply=0;
			cvthdr_v1(h, h1);
			h1.flags=RR_EOF;
			endianize_v1(h1);
			if(_sd)
			{
				send((char *)&h1, sizeof_rrframeheader_v1);
				recv(&reply, 1);
				if(reply==1) {_v.major=1;  _v.minor=0;}
				else if(reply=='V')
				{
					rrversion v;
					_v.id[0]=reply;
					recv((char *)&_v.id[1], sizeof_rrversion-1);
					if(strncmp(_v.id, "VGL", 3) || _v.major<1)
						_throw("Error reading client version");
					v=_v;
					v.major=RR_MAJOR_VERSION;  v.minor=RR_MINOR_VERSION;
					send((char *)&v, sizeof_rrversion);
				}
				if(fconfig.verbose)
					rrout.println("[VGL] Client version: %d.%d", _v.major, _v.minor);
			}
		}
		if((_v.major<2 || (_v.major==2 && _v.minor<1)) && h.compress!=RRCOMP_JPEG)
			_throw("This compression mode requires VirtualGL Client v2.1 or later");
	}
	if(eof) h.flags=RR_EOF;
	if(_dosend)
	{
		if(_v.major==1 && _v.minor==0)
		{
			rrframeheader_v1 h1;
			if(h.dpynum>255) _throw("Display number out of range for v1.0 client");
			cvthdr_v1(h, h1);
			endianize_v1(h1);
			if(_sd)
			{
				send((char *)&h1, sizeof_rrframeheader_v1);
				if(eof)
				{
					char cts=0;
					recv(&cts, 1);
					if(cts<1 || cts>2) _throw("CTS Error");
				}
			}
		}
		else
		{
			endianize(h);
			send((char *)&h, sizeof_rrframeheader);
		}
	}
}


vgltransconn::vgltransconn(void) : _np(fconfig.np),
	_dosend(false), _sd(NULL), _t(NULL), _deadyet(false), _dpynum(0)
{
	memset(&_v, 0, sizeof(rrversion));
	_prof_total.setname("Total(mov)");
}


void vgltransconn::run(void)
{
	rrframe *lastf=NULL, *f=NULL;
	long bytes=0;
	rrtimer t, sleept;  double err=0.;  bool first=true;
	int i;

	try
	{
		vgltranscompressor *c[MAXPROCS];  Thread *ct[MAXPROCS];
		if(fconfig.verbose)
			rrout.println("[VGL] Using %d / %d CPU's for compression",
				_np, numprocs());
		for(i=0; i<_np; i++)
			errifnot(c[i]=new vgltranscompressor(i, this));
		if(_np>1) for(i=1; i<_np; i++)
		{
			errifnot(ct[i]=new Thread(c[i]));
			ct[i]->start();
		}

		while(!_deadyet)
		{
			int np;
			void *ftemp=NULL;
			_q.get(&ftemp);  f=(rrframe *)ftemp;  if(_deadyet) break;
			if(!f) _throw("Queue has been shut down");
			_ready.signal();
			np=_np;  if(f->_h.compress==RRCOMP_YUV) np=1;
			if(np>1)
				for(i=1; i<np; i++) {
					ct[i]->checkerror();  c[i]->go(f, lastf);
				}
			c[0]->compresssend(f, lastf);
			bytes+=c[0]->_bytes;
			if(np>1)
				for(i=1; i<np; i++) {
					c[i]->stop();  ct[i]->checkerror();  c[i]->send();
					bytes+=c[i]->_bytes;
				}

			sendheader(f->_h, true);

			_prof_total.endframe(f->_h.width*f->_h.height, bytes, 1);
			bytes=0;
			_prof_total.startframe();

			if(fconfig.flushdelay>0.)
			{
				long usec=(long)(fconfig.flushdelay*1000000.);
				if(usec>0) usleep(usec);
			}
			if(fconfig.fps>0.)
			{
				double elapsed=t.elapsed();
				if(first) first=false;
				else
				{
					if(elapsed<1./fconfig.fps)
					{
						sleept.start();
						long usec=(long)((1./fconfig.fps-elapsed-err)*1000000.);
						if(usec>0) usleep(usec);
						double sleeptime=sleept.elapsed();
						err=sleeptime-(1./fconfig.fps-elapsed-err);  if(err<0.) err=0.;
					}
				}
				t.start();
			}

			if(lastf) lastf->complete();
			lastf=f;
		}

		for(i=0; i<_np; i++) c[i]->shutdown();
		if(_np>1) for(i=1; i<_np; i++)
		{
			ct[i]->stop();
			ct[i]->checkerror();
			delete ct[i];
		}
		for(i=0; i<_np; i++) delete c[i];

	}
	catch(rrerror &e)
	{
		if(_t) _t->seterror(e);
		_ready.signal();
 		throw;
	}
}


rrframe *vgltransconn::getframe(int w, int h, int ps, int flags,
	bool stereo)
{
	rrframe *f=NULL;
	if(_deadyet) return NULL;
	if(_t) _t->checkerror();
	{
	rrcs::safelock l(_mutex);
	int framei=-1;
	for(int i=0; i<NFRAMES; i++) if(_frame[i].iscomplete()) framei=i;
	if(framei<0) _throw("No free buffers in pool");
	f=&_frame[framei];  f->waituntilcomplete();
	}
	rrframeheader hdr;
	memset(&hdr, 0, sizeof(rrframeheader));
	hdr.height=hdr.frameh=h;
	hdr.width=hdr.framew=w;
	hdr.x=hdr.y=0;
	f->init(hdr, ps, flags, stereo);
	return f;
}


bool vgltransconn::ready(void)
{
	if(_t) _t->checkerror();
	return(_q.items()<=0);
}


void vgltransconn::synchronize(void)
{
	_ready.wait();
}


static void __vgltransconn_spoilfct(void *f)
{
	if(f) ((rrframe *)f)->complete();
}


void vgltransconn::sendframe(rrframe *f)
{
	if(_t) _t->checkerror();
	f->_h.dpynum=_dpynum;
	_q.spoil((void *)f, __vgltransconn_spoilfct);
}


void vgltranscompressor::compresssend(rrframe *f, rrframe *lastf)
{
	bool bu=false;  rrcompframe cf;
	if(!f) return;
	if(f->_flags&RRFRAME_BOTTOMUP) bu=true;
	int tilesizex=fconfig.tilesize? fconfig.tilesize:f->_h.width;
	int tilesizey=fconfig.tilesize? fconfig.tilesize:f->_h.height;
	int i, j, n=0;

	if(f->_h.compress==RRCOMP_YUV)
	{
		_prof_comp.startframe();
		cf=*f;
		_prof_comp.endframe(f->_h.framew*f->_h.frameh, 0, 1);
		_parent->sendheader(cf._h);
		if(_parent->_dosend) _parent->send((char *)cf._bits, cf._h.size);
		return;
	}

	_bytes=0;
	for(i=0; i<f->_h.height; i+=tilesizey)
	{
		int h=tilesizey, y=i;
		if(f->_h.height-i<(3*tilesizey/2)) {h=f->_h.height-i;  i+=tilesizey;}
		for(j=0; j<f->_h.width; j+=tilesizex, n++)
		{
			int w=tilesizex, x=j;
			if(f->_h.width-j<(3*tilesizex/2)) {w=f->_h.width-j;  j+=tilesizex;}
			if(n%_np!=_myrank) continue;
			if(fconfig.interframe)
			{
				if(f->tileequals(lastf, x, y, w, h)) continue;
			}
			rrframe *tile=f->gettile(x, y, w, h);
			rrcompframe *c=NULL;
			if(_myrank>0) {errifnot(c=new rrcompframe());}
			else c=&cf;
			_prof_comp.startframe();
			*c=*tile;
			double frames=(double)(tile->_h.width*tile->_h.height)
				/(double)(tile->_h.framew*tile->_h.frameh);
			_prof_comp.endframe(tile->_h.width*tile->_h.height, 0, frames);
			_bytes+=c->_h.size;  if(c->_stereo) _bytes+=c->_rh.size;
			delete tile;
			if(_myrank==0)
			{
				_parent->sendheader(c->_h);
				if(_parent->_dosend) _parent->send((char *)c->_bits, c->_h.size);
				if(c->_stereo && c->_rbits)
				{
					_parent->sendheader(c->_rh);
					if(_parent->_dosend) _parent->send((char *)c->_rbits, c->_rh.size);
				}
			}
			else
			{	
				store(c);
			}
		}
	}
}


void vgltransconn::send(char *buf, int len)
{
	try
	{
		if(_sd) _sd->send(buf, len);
	}
	catch(...)
	{
		rrout.println("[VGL] ERROR: Could not send data to client.  Client may have disconnected.");
		throw;
	}
}


void vgltransconn::recv(char *buf, int len)
{
	try
	{
		if(_sd) _sd->recv(buf, len);
	}
	catch(...)
	{
		rrout.println("[VGL] ERROR: Could not receive data from client.  Client may have disconnected.");
		throw;
	}
}


void vgltransconn::connect(char *displayname, unsigned short port)
{
	char *servername=NULL;
	try
	{
		if(!displayname || strlen(displayname)<=0)
			_throw("Invalid receiver name");
		char *ptr=NULL;  servername=strdup(displayname);
		if((ptr=strchr(servername, ':'))!=NULL)
		{
			if(strlen(ptr)>1) _dpynum=atoi(ptr+1);
			if(_dpynum<0 || _dpynum>65535) _dpynum=0;
			*ptr='\0';
		}
		if(!strlen(servername) || !strcmp(servername, "unix"))
			{free(servername);  servername=strdup("localhost");}
		errifnot(_sd=new rrsocket((bool)fconfig.ssl));
		try
		{
			_sd->connect(servername, port);
		}
		catch(...)
		{
			rrout.println("[VGL] ERROR: Could not connect to VGL client.  Make sure that vglclient is");
			rrout.println("[VGL]    running and that either the DISPLAY or VGL_CLIENT environment");
			rrout.println("[VGL]    variable points to the machine on which vglclient is running.");
			throw;
		}
		_dosend=true;
		_prof_total.setname("Total     ");
		errifnot(_t=new Thread(this));
		_t->start();
	}
	catch(...)
	{
		if(servername) free(servername);  throw;
	}
	if(servername) free(servername);
}


void vgltranscompressor::send(void)
{
	for(int i=0; i<_storedframes; i++)
	{
		rrcompframe *c=_frame[i];
		errifnot(c);
		_parent->sendheader(c->_h);
		if(_parent->_dosend) _parent->send((char *)c->_bits, c->_h.size);
		if(c->_stereo && c->_rbits)
		{
			_parent->sendheader(c->_rh);
			if(_parent->_dosend) _parent->send((char *)c->_rbits, c->_rh.size);
		}
		delete c;		
	}
	_storedframes=0;
}