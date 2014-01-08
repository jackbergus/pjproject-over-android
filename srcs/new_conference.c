/* $Id: conference.c 3664 2011-07-19 03:42:28Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
//Omissis
/*
 * DON'T GET CONFUSED WITH TX/RX!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. So TX means the bridge
 * is transmitting to the port, RX means the bridge is receiving from the
 * port.
 */


/**
 * This is a port connected to conference bridge.
 */
struct conf_port
{
    pj_str_t		 name;		/**< Port name.			    */
    pjmedia_port	*port;		/**< get_frame() and put_frame()    */
    pjmedia_port_op	 rx_setting;	/**< Can we receive from this port  */
    pjmedia_port_op	 tx_setting;	/**< Can we transmit to this port   */
    unsigned		 listener_cnt;	/**< Number of listeners.	    */
    SLOT_TYPE		*listener_slots;/**< Array of listeners.	    */
    unsigned		 transmitter_cnt;/**<Number of transmitters.	    */

    /* Shortcut for port info. */
    unsigned		 clock_rate;	/**< Port's clock rate.		    */
    unsigned		 samples_per_frame; /**< Port's samples per frame.  */
    unsigned		 channel_count;	/**< Port's channel count.	    */
    unsigned		 bytes_per_sample; /**< jb09: particular bytes_per_sample	    */

    /* Calculated signal levels: */
    unsigned		 tx_level;	/**< Last tx level to this port.    */
    unsigned		 rx_level;	/**< Last rx level from this port.  */

    /* The normalized signal level adjustment.
     * A value of 128 (NORMAL_LEVEL) means there's no adjustment.
     */
    unsigned		 tx_adj_level;	/**< Adjustment for TX.		    */
    unsigned		 rx_adj_level;	/**< Adjustment for RX.		    */

    /* Resample, for converting clock rate, if they're different. */
    pjmedia_resample	*rx_resample;
    pjmedia_resample	*tx_resample;

    /* RX buffer is temporary buffer to be used when there is mismatch
     * between port's sample rate or ptime with conference's sample rate
     * or ptime. The buffer is used for sampling rate conversion AND/OR to
     * buffer the samples until there are enough samples to fulfill a 
     * complete frame to be processed by the bridge.
     *
     * When both sample rate AND ptime of the port match the conference 
     * settings, this buffer will not be created.
     * 
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t		*rx_buf;	/**< The RX buffer.		    */
    unsigned		 rx_buf_cap;	/**< Max size, in samples	    */
    unsigned		 rx_buf_count;	/**< # of samples in the buf.	    */

    /* Mix buf is a temporary buffer used to mix all signal received
     * by this port from all other ports. The mixed signal will be 
     * automatically adjusted to the appropriate level whenever
     * there is possibility of clipping.
     *
     * This buffer contains samples at bridge's clock rate.
     * The size of this buffer is equal to samples per frame of the bridge.
     */

    int			 mix_adj;	/**< Adjustment level for mix_buf.  */
    int			 last_mix_adj;	/**< Last adjustment level.	    */
    pj_int32_t		*mix_buf;	/**< Total sum of signal.	    */

    /* Tx buffer is a temporary buffer to be used when there's mismatch 
     * between port's clock rate or ptime with conference's sample rate
     * or ptime. This buffer is used as the source of the sampling rate
     * conversion AND/OR to buffer the samples until there are enough
     * samples to fulfill a complete frame to be transmitted to the port.
     *
     * When both sample rate and ptime of the port match the bridge's 
     * settings, this buffer will not be created.
     * 
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t		*tx_buf;	/**< Tx buffer.			    */
    unsigned		 tx_buf_cap;	/**< Max size, in samples.	    */
    unsigned		 tx_buf_count;	/**< # of samples in the buffer.    */

    /* When the port is not receiving signal from any other ports (e.g. when
     * no other ports is transmitting to this port), the bridge periodically
     * transmit NULL frame to the port to keep the port "alive" (for example,
     * a stream port needs this heart-beat to periodically transmit silence
     * frame to keep NAT binding alive).
     *
     * This NULL frame should be sent to the port at the port's ptime rate.
     * So if the port's ptime is greater than the bridge's ptime, the bridge
     * needs to delay the NULL frame until it's the right time to do so.
     *
     * This variable keeps track of how many pending NULL samples are being
     * "held" for this port. Once this value reaches samples_per_frame
     * value of the port, a NULL frame is sent. The samples value on this
     * variable is clocked at the port's clock rate.
     */
    unsigned		 tx_heart_beat;

    /* Delay buffer is a special buffer for sound device port (port 0, master
     * port) and other passive ports (sound device port is also passive port).
     *
     * We need the delay buffer because we can not expect the mic and speaker 
     * thread to run equally after one another. In most systems, each thread 
     * will run multiple times before the other thread gains execution time. 
     * For example, in my system, mic thread is called three times, then 
     * speaker thread is called three times, and so on. This we call burst.
     *
     * There is also possibility of drift, unbalanced rate between put_frame
     * and get_frame operation, in passive ports. If drift happens, snd_buf
     * needs to be expanded or shrinked. 
     *
     * Burst and drift are handled by delay buffer.
     */
    pjmedia_delay_buf	*delay_buf;
};


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    unsigned		  options;	/**< Bitmask options.		    */
    unsigned		  max_ports;	/**< Maximum ports.		    */
    unsigned		  port_cnt;	/**< Current number of ports.	    */
    unsigned		  connect_cnt;	/**< Total number of connections    */
    pjmedia_snd_port	 *snd_dev_port;	/**< Sound device port.		    */
    pjmedia_port	 *master_port;	/**< Port zero's port.		    */
    char		  master_name_buf[80]; /**< Port0 name buffer.	    */
    pj_mutex_t		 *mutex;	/**< Conference mutex.		    */
    struct conf_port	**ports;	/**< Array of ports.		    */
    unsigned		  clock_rate;	/**< Sampling rate.		    */
    unsigned		  channel_count;/**< Number of channels (1=mono).   */
    unsigned		  samples_per_frame;	/**< Samples per frame.	    */
    unsigned		  bits_per_sample;	/**< Bits per sample.	    */
};


/* Prototypes */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame);
static pj_status_t get_frame_pasv(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t destroy_port(pjmedia_port *this_port);
static pj_status_t destroy_port_pasv(pjmedia_port *this_port);


/*
 * Create port.
 */
static pj_status_t create_conf_port( pj_pool_t *pool,
				     pjmedia_conf *conf,
				     pjmedia_port *port,
				     const pj_str_t *name,
				     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port;
    pj_status_t status;

    /* Create port. */
    conf_port = PJ_POOL_ZALLOC_T(pool, struct conf_port);
    PJ_ASSERT_RETURN(conf_port, PJ_ENOMEM);

    /* Set name */
    pj_strdup_with_null(pool, &conf_port->name, name);

    /* Default has tx and rx enabled. */
    conf_port->rx_setting = PJMEDIA_PORT_ENABLE;
    conf_port->tx_setting = PJMEDIA_PORT_ENABLE;

    /* Default level adjustment is 128 (which means no adjustment) */
    conf_port->tx_adj_level = NORMAL_LEVEL;
    conf_port->rx_adj_level = NORMAL_LEVEL;

    /* Create transmit flag array */
    conf_port->listener_slots = (SLOT_TYPE*)
				pj_pool_zalloc(pool, 
					  conf->max_ports * sizeof(SLOT_TYPE));
    PJ_ASSERT_RETURN(conf_port->listener_slots, PJ_ENOMEM);

    /* Save some port's infos, for convenience. */
    if (port) {
	pjmedia_audio_format_detail *afd;

	afd = pjmedia_format_get_audio_format_detail(&port->info.fmt, 1);
	conf_port->port = port;
	conf_port->clock_rate = afd->clock_rate;
	conf_port->samples_per_frame = PJMEDIA_AFD_SPF(afd);
	conf_port->channel_count = afd->channel_count;
	conf_port->bytes_per_sample = (afd->bits_per_sample / 8); //jb09
    } else {
	conf_port->port = NULL;
	conf_port->clock_rate = conf->clock_rate;
	conf_port->samples_per_frame = conf->samples_per_frame;
	conf_port->channel_count = conf->channel_count;
	conf_port->bytes_per_sample = BYTES_PER_SAMPLE;		//jb09
    }

    /* If port's clock rate is different than conference's clock rate,
     * create a resample sessions.
     */
    if (conf_port->clock_rate != conf->clock_rate) {

	pj_bool_t high_quality;
	pj_bool_t large_filter;

	high_quality = ((conf->options & PJMEDIA_CONF_USE_LINEAR)==0);
	large_filter = ((conf->options & PJMEDIA_CONF_SMALL_FILTER)==0);

	/* Create resample for rx buffer. */
	status = pjmedia_resample_create( pool, 
					  high_quality,
					  large_filter,
					  conf->channel_count,
					  conf_port->clock_rate,/* Rate in */
					  conf->clock_rate, /* Rate out */
					  conf->samples_per_frame * 
					    conf_port->clock_rate /
					    conf->clock_rate,
					  &conf_port->rx_resample);
	if (status != PJ_SUCCESS)
	    return status;


	/* Create resample for tx buffer. */
	status = pjmedia_resample_create(pool,
					 high_quality,
					 large_filter,
					 conf->channel_count,
					 conf->clock_rate,  /* Rate in */
					 conf_port->clock_rate, /* Rate out */
					 conf->samples_per_frame,
					 &conf_port->tx_resample);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /*
     * Initialize rx and tx buffer, only when port's samples per frame or 
     * port's clock rate or channel number is different then the conference
     * bridge settings.
     */
    if (conf_port->clock_rate != conf->clock_rate ||
	conf_port->channel_count != conf->channel_count ||
	conf_port->samples_per_frame != conf->samples_per_frame)
    {
	unsigned port_ptime, conf_ptime, buff_ptime, dbld = 1;

	port_ptime = conf_port->samples_per_frame / conf_port->channel_count *
	    1000 / conf_port->clock_rate;
	conf_ptime = conf->samples_per_frame / conf->channel_count *
	    1000 / conf->clock_rate;

PJ_LOG(4,(THIS_FILE, "conf_port->samples_per_frame %u, conf_port->channel_count %u, conf_port->clock_rate %u, port_ptime %u, conf_ptime %u\n",(unsigned int)conf_port->samples_per_frame, (unsigned int)conf_port->channel_count, conf_port->clock_rate, port_ptime , conf_ptime));//jb09

	/* Calculate the size (in ptime) for the port buffer according to
	 * this formula:
	 *   - if either ptime is an exact multiple of the other, then use
	 *     the larger ptime (e.g. 20ms and 40ms, use 40ms).
	 *   - if not, then the ptime is sum of both ptimes (e.g. 20ms
	 *     and 30ms, use 50ms)
	 */
	 
	 //jb09: START
	if (port_ptime > conf_ptime) {
	    buff_ptime = port_ptime;
	    
	   /* if (port_ptime % conf_ptime)
		buff_ptime += conf_ptime;
	else PJ_LOG(4,(THIS_FILE, "no inner if "));*/
	
	} else {
	    buff_ptime = conf_ptime;
	    /* if (conf_ptime % port_ptime)
		buff_ptime += port_ptime;
		else PJ_LOG(4,(THIS_FILE, "no inner if")); */
	}
	
	if (port_ptime % conf_ptime) dbld = 2;
	    buff_ptime *= dbld;
	    PJ_LOG(4,(THIS_FILE, "case 1: port_ptime %u, conf_ptime %u, buff_ptime %u\n", port_ptime, conf_ptime, buff_ptime));
//jb09:END
	/* Create RX buffer. */
	//conf_port->rx_buf_cap = (unsigned)(conf_port->samples_per_frame +
	//				   conf->samples_per_frame * 
	//				   conf_port->clock_rate * 1.0 /
	//				   conf->clock_rate + 0.5);
	
#define MYMAX(a,b) ((a)>(b) ? a : b)
	
	conf_port->rx_buf_cap = 2 * conf_port->samples_per_frame * dbld;//jb09
	if (conf_port->channel_count > conf->channel_count)
	    conf_port->rx_buf_cap *= conf_port->channel_count;
	else
	    conf_port->rx_buf_cap *= conf->channel_count;

	conf_port->rx_buf_count = 0;
	conf_port->rx_buf = (pj_int16_t*)
			    pj_pool_alloc(pool, conf_port->rx_buf_cap *
						sizeof(conf_port->rx_buf[0]));
	PJ_ASSERT_RETURN(conf_port->rx_buf, PJ_ENOMEM);

	/* Create TX buffer. */
	conf_port->tx_buf_cap = conf_port->rx_buf_cap;
	conf_port->tx_buf_count = 0;
	conf_port->tx_buf = (pj_int16_t*)
			    pj_pool_alloc(pool, conf_port->tx_buf_cap *
						sizeof(conf_port->tx_buf[0]));
	PJ_ASSERT_RETURN(conf_port->tx_buf, PJ_ENOMEM);
    }


    /* Create mix buffer. */
    conf_port->mix_buf = (pj_int32_t*)
			 pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->mix_buf[0]));
    PJ_ASSERT_RETURN(conf_port->mix_buf, PJ_ENOMEM);
    conf_port->last_mix_adj = NORMAL_LEVEL;


    /* Done */
    *p_conf_port = conf_port;
    return PJ_SUCCESS;
}


//Omissis


/*
 * Read from port.
 */
static pj_status_t read_port( pjmedia_conf *conf,
			      struct conf_port *cport, pj_int16_t *frame,
			      pj_size_t count, pjmedia_frame_type *type )
{

    pj_assert(count == conf->samples_per_frame);
    

    TRACE_((THIS_FILE, "read_port %.*s: count=%d", 
		       (int)cport->name.slen, cport->name.ptr,
		       count));

    /* 
     * If port's samples per frame and sampling rate and channel count
     * matche conference bridge's settings, get the frame directly from
     * the port.
     */
    if (cport->rx_buf_cap == 0) {
	pjmedia_frame f;
	pj_status_t status;

	f.buf = frame;
	f.size = count * cport->bytes_per_sample; //XXX: VALORE DI DEFAULT BYTES_PER_SAMPLE (jb09)

	TRACE_((THIS_FILE, "  get_frame %.*s: count=%d", 
		   (int)cport->name.slen, cport->name.ptr,
		   count));

	status = pjmedia_port_get_frame(cport->port, &f);

	*type = f.type;

	return status;

    } else {
	unsigned samples_req;

	/* Initialize frame type */
	if (cport->rx_buf_count == 0) {
	    *type = PJMEDIA_FRAME_TYPE_NONE;
	} else {
	    /* we got some samples in the buffer */
	    *type = PJMEDIA_FRAME_TYPE_AUDIO;
	}

	/*
	 * If we don't have enough samples in rx_buf, read from the port 
	 * first. Remember that rx_buf may be in different clock rate and
	 * channel count!
	 */

	samples_req = (unsigned) (count * 1.0 * 
		      cport->clock_rate / conf->clock_rate + 0.5);


	while (cport->rx_buf_count < samples_req) {




	    pjmedia_frame f;
	    pj_status_t status;
	    int spf = cport->samples_per_frame; //(jb09)
	    int tmpsize = cport->samples_per_frame * cport->bytes_per_sample; //(jb09)

        //(jb09) start
	if (samples_req > cport->rx_buf_cap) {
	PJ_LOG(4,(THIS_FILE, "WARNING: ASSERTING. bufcount = %u, bufcap = %u, tmpsize=%u, spf=%u, count=%u, cpclock=%u, cclock=%u\n",
				     cport->rx_buf_count, cport->rx_buf_cap,tmpsize,spf,count,cport->clock_rate / conf->clock_rate));
				     
				     pj_assert(samples_req <= cport->rx_buf_cap);
	}
	

	    f.buf = cport->rx_buf + cport->rx_buf_count;
	    if (cport->rx_buf_count + tmpsize > cport->rx_buf_cap) {
	    	PJ_LOG(4,(THIS_FILE, "WARNING: EXCEEDING. bufcount = %u, bufcap = %u, tmpsize=%u, spf=%u\n",
				     cport->rx_buf_count, cport->rx_buf_cap,tmpsize,spf));
				     //BREAK;
	    }//(jb09) end
	    
	    f.size = tmpsize;

	    TRACE_((THIS_FILE, "  get_frame, count=%d", 
		       cport->samples_per_frame));

	    status = pjmedia_port_get_frame(cport->port, &f);

	    if (status != PJ_SUCCESS) {
		/* Fatal error! */
		return status;
	    }

	    if (f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
		TRACE_((THIS_FILE, "  get_frame returned non-audio"));
		pjmedia_zero_samples( cport->rx_buf + cport->rx_buf_count,
				      spf);//(jb09)
	    } else {
		/* We've got at least one frame */
		*type = PJMEDIA_FRAME_TYPE_AUDIO;
	    }

	    /* Adjust channels */
	    if (cport->channel_count != conf->channel_count) {
		if (cport->channel_count == 1) {
		    pjmedia_convert_channel_1ton((pj_int16_t*)f.buf, 
						 (const pj_int16_t*)f.buf,
						 conf->channel_count, 
						 spf,//(jb09)
						 0);
		    cport->rx_buf_count += (spf * conf->channel_count);//(jb09)
		} else { /* conf->channel_count == 1 */
		    pjmedia_convert_channel_nto1((pj_int16_t*)f.buf, 
						 (const pj_int16_t*)f.buf,
						 cport->channel_count, 
						 spf, //(jb09)
						 PJMEDIA_STEREO_MIX, 0);
		    cport->rx_buf_count += (spf / cport->channel_count);//(jb09)
		}
	    } else {
		cport->rx_buf_count += cport->samples_per_frame;
	    }

	    TRACE_((THIS_FILE, "  rx buffer size is now %d",
		    cport->rx_buf_count));

	PJ_LOG(4,(THIS_FILE, "bufcount = %u, bufcap = %u, tmpsize=%u, spf=%u\n",
				     cport->rx_buf_count, cport->rx_buf_cap,tmpsize,spf));

	    pj_assert(cport->rx_buf_count <= cport->rx_buf_cap);
	}

	/*
	 * If port's clock_rate is different, resample.
	 * Otherwise just copy.
	 */
	if (cport->clock_rate != conf->clock_rate) {
	    
	    unsigned src_count;

	    TRACE_((THIS_FILE, "  resample, input count=%d", 
		    pjmedia_resample_get_input_size(cport->rx_resample)));

	    pjmedia_resample_run( cport->rx_resample,cport->rx_buf, frame);

	    src_count = (unsigned)(count * 1.0 * cport->clock_rate / 
				   conf->clock_rate + 0.5);
	    cport->rx_buf_count -= src_count;
	    if (cport->rx_buf_count) {
		pjmedia_move_samples(cport->rx_buf, cport->rx_buf+src_count,
				     cport->rx_buf_count);
	    }

	    TRACE_((THIS_FILE, "  rx buffer size is now %d",
		    cport->rx_buf_count));

	} else {

	    pjmedia_copy_samples(frame, cport->rx_buf, count);
	    cport->rx_buf_count -= count;
	    if (cport->rx_buf_count) {
		pjmedia_move_samples(cport->rx_buf, cport->rx_buf+count,
				     cport->rx_buf_count);
	    }
	}
    }

    return PJ_SUCCESS;
}
//Omissis
