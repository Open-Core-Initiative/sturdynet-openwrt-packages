/*TCP WRC - Wireless RTT base Control
 *	Based on Congestion avoidance algortihm proposed by Verizon
 *	Max and Min congestion window limits based on RTT
 *	values. Two RTT markers ties the values of Max and
 *	Min Cwnd. In between RTT values, calculate the Cwnd based on
 *	slope between Max and Min Cwnd values.
 *	Please refer "Requirements for New TCP Congestion Avoidance
 *      Module" doc, by Vz Labs
 *	Assumption - Since congestion window operates on MSS,
 *	all cwnd calculations are in terms of segments.
 *
 *	Module Parameters
 *  	cwnd_highbound - Maximum possible value of Cwnd in Bytes
 *      cwnd_lowbound - Maximum value of Cwnd (Bytes) when RTT crosses high mark
 *	rtt_lowvaluemarker(ms)-RTT marker below which Cwnd stays at cwnd_highbound
 *	rtt_highvaluemarke(ms)-RTT marker after which Cwnd stays at cwnd_lowbound
 *
 *	Global Variables
 *	maxcwnd_rttslope - Calculated slope of max Cwnd decline between
 *	rtt_lowvaluemarker and rtt_highvaluemarker
 *	tcp_rbc_lpf_rtt_thresh(ms) - High RTT values above this value
 *	tcp_rbc_rtt_ewma_weight	- Weight for EWMA RTT
 *   	tcp_rbc_rtt_ewma_light_weight - Weight for using High RTT values
 *
 *	Important Functions
 *	tcp_rbc_maxcwndrttslope(void)-slope of max cwnd with rtti.e. y2-y1/x2-x1
 *
 *	get_cwnd_inbound_sgmnts(u32 crntrtt, u32 mss) - get cwnd max size when
 *	rtt_lowvaluemarker<RTT<rtt_highvaluemarker. Called each time RTT value
 * 	is re-calculated
 *
 *	tcp_rbc_lpf_srtt(s32 rtt_ms, u32 srtt_prev) - Verizon version of
 *	calculating RTT. Called upon rx of ack. Follow EWMA formula
 * 	  rtt =  prev_rtt*(1-w) + crnt_rtt*w
 *	value of w can be 1,3, or 12 dependening upon crnt_rtt
 */


/* Note: WRC version aligns with the build version with WASP.*/
#define WRC_VERSION   "2.3.1"


#include <net/tcp.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/version.h>

#define AQM_THRESHOLD 500

#ifdef  AQM_THRESHOLD 
static u32 cwnd_highbound = 14680064;
static u32 cwnd_lowbound = 1048576;
static u32 rtt_lowvaluemarker_floor = 550;
static u32 rtt_lowvaluemarker_ceiling = 650;
static u32 rtt_highvaluemarker_floor = 1500;
static u32 rtt_highvaluemarker_ceiling = 2500;
static u32 tcp_rbc_lpf_rtt_thresh = 3000; 
#else
static u32 cwnd_highbound = 720896;
static u32 cwnd_lowbound = 131072;
static u32 rtt_lowvaluemarker_floor = 30;
static u32 rtt_lowvaluemarker_ceiling = 100;
static u32 rtt_highvaluemarker_floor = 300;
static u32 rtt_highvaluemarker_ceiling = 900;
static u32 tcp_rbc_lpf_rtt_thresh = 500;
#endif


static u32 tcp_rbc_rtt_ewma_weight = 3;
static u32 tcp_rbc_rtt_ewma_light_weight = 1;
static u32 tcp_rbc_rtt_ewma_heavy_weight = 12;
static u32 tcp_rbc_rtt_max_weight = 100;

static u32 cwnd_safe_factor = 90;
static u32 init_cwnd = 512;
static u32 quiet = 1;
static u32 slow_start_check_log = 1;
static u32 rtt_high_factor = 4;
static u32 min_rtt_check_threshold = 10;

static u32 conservative_factor = 10;
static u32 ca_enter_threshold = 1500;
static u32 ca_exit_threshold = 2000;
static u32 ca_increase_step = 10;
static u32 legacy_mode = 1;
static u32 rwin_booster = 1;

static u32 rwin_booster_safe_factor = 3;

module_param(cwnd_highbound, uint, 0644);
MODULE_PARM_DESC(cwnd_highbound, " max upper bound of cwnd in bytes");
module_param(cwnd_lowbound, uint, 0644);
MODULE_PARM_DESC(cwnd_lowbound, " max value of cwnd when rtt is greater than rtt_high_thresh");

module_param(rtt_lowvaluemarker_floor, uint, 0644);
MODULE_PARM_DESC(rtt_lowvaluemarker_floor, " the left point of low value marker for rtt");
module_param(rtt_lowvaluemarker_ceiling, uint, 0644);
MODULE_PARM_DESC(rtt_lowvaluemarker_ceiling, " the right point of low value marker for rtt");

module_param(rtt_highvaluemarker_floor, uint, 0644);
MODULE_PARM_DESC(rtt_highvaluemarker_floor, " the left point of high value marker for rtt");
module_param(rtt_highvaluemarker_ceiling, uint, 0644);
MODULE_PARM_DESC(rtt_highvaluemarker_ceiling, " the right point of high value marker for rtt");

module_param(tcp_rbc_lpf_rtt_thresh, uint, 0644);
MODULE_PARM_DESC(tcp_rbc_lpf_rtt_thresh, " the high rtt threshold used for low pass filter");

module_param(init_cwnd, uint, 0644);
MODULE_PARM_DESC(init_cwnd, " initial cwnd size");

module_param(cwnd_safe_factor, uint, 0644);
MODULE_PARM_DESC(cwnd_safe_factor, " limit the maximum cwnd up to a fraction of rwin");

module_param(rtt_high_factor, uint, 0644);
MODULE_PARM_DESC(rtt_high_factor, " high rtt threshold factor");

module_param(quiet, uint, 0644);
MODULE_PARM_DESC(quiet, " disable debug infomation");

module_param(slow_start_check_log, uint, 0644);
MODULE_PARM_DESC(slot_start_check_log, " flag logging for slow_start condition check, default is 1 enabled");

module_param(min_rtt_check_threshold, uint, 0644);
MODULE_PARM_DESC(min_rtt_check_threshold, " check the first N RTTs to decides the min_rtt");

module_param(conservative_factor, uint, 0644);
MODULE_PARM_DESC(conservative_factor, " factor used to exit from congestion avoidance");

module_param(ca_enter_threshold, uint, 0644);
MODULE_PARM_DESC(ca_enter_threshold, " congestion avoidance enter threshold (ms)");

module_param(ca_exit_threshold, uint, 0644);
MODULE_PARM_DESC(ca_exit_threshold, " congestion avoidance exit threshold (ms)");

module_param(ca_increase_step, uint, 0644);
MODULE_PARM_DESC(ca_increase_step, " cwnd increasement in congestion avoidance mode");

module_param(rwin_booster, uint, 0644);
MODULE_PARM_DESC(rwin_booster, " flag to enable rwin booster ");

module_param(rwin_booster_safe_factor, uint, 0644);
MODULE_PARM_DESC(rwin_booster_safe_factor, " scale factor to avoid SWS. (suggested not change)");

module_param(legacy_mode, uint, 0644);
MODULE_PARM_DESC(legacy_mode, " legacy mode ");

struct rbctcp {
	u32 maxcwnd;
	u32 mincwnd;
	u32 prev_cwnd;
	u32 srtt;
	u32 init_rtt;
	u32 rtt_highvaluemarker;
	u32 rtt_lowvaluemarker;
	s32 cwndslope;
	u32 min_rtt;
	u64 rtt_cnt;
	u32 start_tm;
	u32 enter_ca_tm;
        u32 rwin_booster;
};

static inline u32 get_cwnd_ubound_sgmnts(u32 mss)
{
	return (cwnd_highbound / mss);
}

static inline u32 get_cwnd_lbound_sgmnts(u32 mss)
{
	return (cwnd_lowbound / mss);
}

static u32 get_cwnd_inbound_sgmnts(struct rbctcp *ca, u32 crntrtt, u32 mss)
{
	const u32 rtt_highvaluemarker = ca->rtt_highvaluemarker;
	const u32 rtt_lowvaluemarker = ca->rtt_lowvaluemarker;
	const s32 maxcwnd_rttslope = ca->cwndslope;

	if (crntrtt >= rtt_highvaluemarker)
		return get_cwnd_lbound_sgmnts(mss);
	else if (crntrtt <= rtt_lowvaluemarker)
		return get_cwnd_ubound_sgmnts(mss);
	else {
		u32 crntcwnd_bytes =
			cwnd_highbound +
			((crntrtt - rtt_lowvaluemarker) * maxcwnd_rttslope);

		return (crntcwnd_bytes / mss);
	}
}

static inline s64 gettime_us(void)
{
	return ktime_to_us(ktime_get_real());
}

static inline u32 gettime_ms(void)
{
	return ktime_to_ms(ktime_get_real());
}

static inline u32 get_rtt_us(const struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,14,0)
	const u32 srtt_us = jiffies_to_usecs(tp->srtt) >> 3;
#else
	const u32 srtt_us = tp->srtt_us >> 3;
#endif
	return srtt_us;
}

static inline u32 get_rtt_ms(const struct sock *sk)
{
	const u32 srtt_us = get_rtt_us(sk);
	return (srtt_us / 1000);
}

static inline u32 get_current_rate_kbps(const struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	const u32 mss_cache = tp->mss_cache;
	const u32 in_flight = tcp_packets_in_flight(tp);
	const u32 srtt_ms = get_rtt_ms(sk);

	u32 rate = (in_flight * mss_cache * 8 ) * (srtt_ms) / ( 1000 * 1024);

	return rate;
}

static u64 tcp_compute_delivery_rate(const struct tcp_sock *tp)
{
        u32 rate = READ_ONCE(tp->rate_delivered);
        u32 intv = READ_ONCE(tp->rate_interval_us);
        u64 rate64 = 0;

        if (rate && intv) {
                rate64 = (u64)rate * tp->mss_cache * USEC_PER_SEC;
                do_div(rate64, intv);
        }
        return rate64;
}

static inline void print_tcp_sock_hdr(struct sock *sk, const char *func, const u32 line) {

	const struct tcp_sock *tp = tcp_sk(sk);
	const struct rbctcp *ca = inet_csk_ca(sk);
	const u32 now = gettime_ms() - ca->start_tm;

	pr_info("time_elapsed_ms:%u, ", now);
	pr_cont("func:%s, line:%u, ", func, line);
	pr_cont("%pI4:%d -> %pI4:%d, ", &((struct inet_sock *)tp)->inet_saddr,
                       ntohs(((struct inet_sock *)tp)->inet_sport),
                       &((struct inet_sock *)tp)->inet_daddr,
                       ntohs(((struct inet_sock *)tp)->inet_dport));
	pr_cont("stt_ms: %lu, ", (tp->srtt_us >> 3) / USEC_PER_MSEC);
	pr_cont("rtt_min_ms: %ld, ", tcp_min_rtt(tp) / USEC_PER_MSEC); 
	pr_cont("snd_cwnd: %u, ", tp->snd_cwnd); 
	pr_cont("snd_cwnd_clamp: %u, ", tp->snd_cwnd_clamp); 
	pr_cont("snd_ssthresh: %u, ", tp->snd_ssthresh); 
	pr_cont("delivery_rate: %llu, ", tcp_compute_delivery_rate(tp)); 
	pr_cont("snd_wnd(adv_wnd): %u, ", tp->snd_wnd);
	pr_cont("max_window: %u, ", tp->max_window);
	pr_cont("pkts_out: %u, ", tp->packets_out);
	pr_cont("pkts_in_flight: %u, ", tcp_packets_in_flight(tp));
	pr_cont("ca_min_rtt_ms: %u, ", ca->min_rtt);
	pr_cont("ca_init_rtt_ms: %u, ", ca->init_rtt); 
	pr_cont("ca_rtt_high_marker: %u, ", ca->rtt_highvaluemarker);
	pr_cont("ca_rtt_low_marker: %u, ", ca->rtt_lowvaluemarker);
	pr_cont("ca_cwndslope: %d, ", ca->cwndslope);
	pr_cont("ca_maxcwnd: %d, ", ca->maxcwnd);
	pr_cont("ca_mincwnd: %d, ", ca->mincwnd);
	
	return;
}
//////////////////////////////////////////////////////////////////////////////

static void tcp_rbc_reset(struct rbctcp *ca, u32 mss)
{
	ca->maxcwnd = get_cwnd_ubound_sgmnts(mss);
	ca->mincwnd = get_cwnd_lbound_sgmnts(mss);
}

static void tcp_rbc_init(struct sock *sk)
{
	struct rbctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	tcp_rbc_reset(ca, tp->mss_cache);

	tp->snd_cwnd = init_cwnd;
	tp->snd_cwnd_clamp = get_cwnd_ubound_sgmnts(tp->mss_cache);
	tp->snd_ssthresh = TCP_INFINITE_SSTHRESH;

	ca->start_tm = gettime_ms();
	/* set init rtt as 0 */
	ca->init_rtt = 0;
	/* set min rtt as a rtt_highvaluemarker */
	ca->min_rtt = rtt_highvaluemarker_ceiling;
	/* set rtt counter as false(0) */
	ca->rtt_cnt = 0;

	ca->enter_ca_tm = 0;

	ca->prev_cwnd = tp->snd_cwnd_clamp;

        ca->rwin_booster = rwin_booster;

	if (!quiet) {
		pr_info("RBC TCP Initalized, sk:%p, mss: %u, maxwind: %u, cwnd: %u\n",
		       sk, tp->mss_cache, tp->snd_cwnd_clamp, tp->snd_cwnd);
		print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
	}
}

void tcp_rbc_slow_start(struct tcp_sock *tp)
{

	int cnt; /* increase in packets */

	cnt = tp->snd_cwnd;	             /* exponential increase */
	tp->snd_cwnd_cnt += cnt;

	while (tp->snd_cwnd_cnt >= tp->snd_cwnd) {
		tp->snd_cwnd_cnt -= tp->snd_cwnd;
		if (tp->snd_cwnd < tp->snd_cwnd_clamp)
			tp->snd_cwnd++;
	}
}


void tcp_rbc_cong_avoid_ai(struct tcp_sock *tp, u32 w)
{
	if (tp->snd_cwnd_cnt >= w) {
		if (tp->snd_cwnd < tp->snd_cwnd_clamp)
			tp->snd_cwnd += ca_increase_step; /* default is 2pkts/RTT */
		tp->snd_cwnd_cnt = 0;
	} else {
		tp->snd_cwnd_cnt++;
	}
}

/**
  *  force_check_cwnd is only required for 3.2.0 kernel, which has bug to trigger
  *  CPU softlockup when snd_cwnd = 0 and snd_cwnd_clamp = 0.
  */

static inline void force_check_cwnd(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	 /*
	 * snd_cwnd==0 and snd_cwnd_clamp==0, would cause CPU soft lock up in
	 * tcp_slow_start().
	 */
	if (unlikely(tp->snd_cwnd == 0)) {
		if (slow_start_check_log) {
			pr_emerg("WRC snd_cwnd: snd_cwnd: %u, snd_cwnd_clamp: %u. ssthresh: %u\n",
				 tp->snd_cwnd, tp->snd_cwnd_clamp, tp->snd_ssthresh);
			print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
		}
		/* RUP: setting snd_cwnd from 0 to init_cwnd, would cause potential
		 * packet drop.
		 */
		if (tp->snd_ssthresh > 0 && tp->snd_ssthresh < init_cwnd) {
			tp->snd_cwnd = tp->snd_ssthresh;
		} else {
			const u32 init_cwnd_default = TCP_INIT_CWND;
			tp->snd_cwnd = max(init_cwnd, init_cwnd_default);
		}
	}
	if (unlikely(tp->snd_cwnd_clamp == 0)) {
		if (slow_start_check_log) {
			pr_emerg("WRC snd_cwnd_clamp: snd_cwnd: %u, snd_cwnd_clamp: %u. ssthresh: %u\n",
				 tp->snd_cwnd, tp->snd_cwnd_clamp, tp->snd_ssthresh);
			print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
		}
		tp->snd_cwnd_clamp = get_cwnd_lbound_sgmnts(tp->mss_cache);
	}

	return;
}


static inline void tcp_rbc_quick_start(struct sock *sk) {

        struct tcp_sock *tp = tcp_sk(sk);
	struct rbctcp *ca = inet_csk_ca(sk);
	const u32 cwnd_low_bound = get_cwnd_lbound_sgmnts(tp->mss_cache); 
	// u32 rwnd = (tp->max_window - 2 * tp->mss_cache) / tp->mss_cache; 
	u32 rwnd = tp->max_window / (tp->mss_cache);
	u32 cwnd_clamp = tp->snd_cwnd_clamp;
        u32 cwnd_clamp_bytes = cwnd_clamp * tp->mss_cache;
	
	//adjust rwnd in packets to avoid SWS 
	rwnd = rwnd - (rwnd >> rwin_booster_safe_factor); 
       	rwnd = (rwnd > TCP_INIT_CWND) ? rwnd : TCP_INIT_CWND;

        cwnd_clamp = max(cwnd_clamp, cwnd_low_bound); // safe check 
	tp->snd_cwnd = min(rwnd, cwnd_clamp);
	tp->snd_cwnd_cnt = 0;

        // rwin booster only used when UE's rwin is not opened up.
        if (tp->max_window >= cwnd_clamp_bytes * 2) {
                //rwin is large enough, exit from rwin booster mode.
                ca->rwin_booster = 0;
		print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__); 
		pr_cont("rwin_booster: %u, ", ca->rwin_booster);
        }

        return;
}


static void tcp_rbc_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct rbctcp *ca = inet_csk_ca(sk);
	const u32 delta_ms = (ca->enter_ca_tm == 0) ? \
				0 : gettime_ms() - ca->enter_ca_tm;
	const u32 srtt_ms = ca->srtt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,15,0)
	if (!tcp_is_cwnd_limited(sk, in_flight))
		return;
#else
	if (!tcp_is_cwnd_limited(sk))
		return;
#endif
	/* exit check from congestion avoidance state */
	if (unlikely(tp->snd_cwnd >= tp->snd_ssthresh)) {
		if ((srtt_ms < ca_exit_threshold) &&
			(delta_ms > srtt_ms * conservative_factor)) {
			tp->snd_ssthresh = TCP_INFINITE_SSTHRESH;
			ca->enter_ca_tm = 0 ;  // reset
			if (!quiet) {
				pr_warn("sk: %p exists from congestion avoidance.", sk);
				print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
			}
		}
	}

	if (tp->snd_cwnd < tp->snd_ssthresh) {
		/* force_check_cwnd_only used for 3.2 kernel */
		force_check_cwnd(sk);

		if (ca->rwin_booster == 1) {
			/* we are in initial slow start and rwin is not large enough */
			tcp_rbc_quick_start(sk);
			if (!quiet) {
				pr_warn("sk: %p used quick/rwin-booster.", sk);
				print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
			}
		} else if (legacy_mode) {
                        /* using rbc slow start */
			tcp_rbc_slow_start(tp);
		} else {
			/* using slow start from tcp_cong.c */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,0)
			tcp_slow_start(tp);
#else
			tcp_slow_start(tp, in_flight);
#endif
		}
	} else {
                /* congestion avoidance */
		if (ca->enter_ca_tm == 0) {
			//mark the first time enter ca
			ca->enter_ca_tm = gettime_ms();
		}

		if (legacy_mode) {
			tcp_rbc_cong_avoid_ai(tp, tp->snd_cwnd);
		} else {
			/* using cong_avoid_ai from tcp_cong.c */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,18,0)
			tcp_cong_avoid_ai(tp, tp->snd_cwnd);
#else
			tcp_cong_avoid_ai(tp, tp->snd_cwnd, in_flight);
#endif
		}
	}

}

static inline u32 tcp_rbc_ssthresh(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u32 newssthresh = (tp->snd_cwnd * 2) / 3;
	struct rbctcp *ca = inet_csk_ca(sk);
	ca->prev_cwnd = tp->snd_cwnd;

	return max(newssthresh, init_cwnd);
}

static inline u32 tcp_rbc_lpf_srtt(struct rbctcp *ca, s32 rtt_ms, u32 srtt_prev)
{

	u32 srtt_crnt = srtt_prev;
	u32 maxweight = tcp_rbc_rtt_max_weight;

	const u32 rtt_lowvaluemarker = ca->rtt_lowvaluemarker;
	const u32 rtt_highvaluemarker = ca->rtt_highvaluemarker;

	if (rtt_ms <= 0) {
		return srtt_crnt;
	}

	if (rtt_ms > tcp_rbc_lpf_rtt_thresh) {
		u32 weight = tcp_rbc_rtt_ewma_light_weight;
		srtt_crnt =
			(srtt_prev * (maxweight - weight) +
			 rtt_ms * weight) / maxweight;
		return srtt_crnt;
	}

	if (srtt_prev < rtt_lowvaluemarker) {
		u32 weight = tcp_rbc_rtt_ewma_heavy_weight;
		srtt_crnt =
			(srtt_prev * (maxweight - weight) +
			 rtt_ms * weight) / maxweight;
		return srtt_crnt;
	}

	if ((srtt_prev >= rtt_lowvaluemarker)
	    && (srtt_prev < rtt_highvaluemarker)) {
		u32 weight = tcp_rbc_rtt_ewma_weight;
		srtt_crnt =
			(srtt_prev * (maxweight - weight) +
			 rtt_ms * weight) / maxweight;
		return srtt_crnt;
	}

	if (srtt_prev >= rtt_highvaluemarker) {
		u32 weight = tcp_rbc_rtt_ewma_heavy_weight;
		srtt_crnt =
			(srtt_prev * (maxweight - weight) +
			 rtt_ms * weight) / maxweight;
		return srtt_crnt;
	}

	return srtt_crnt;
}

static inline u32 get_rtt_high_watermarker(u32 rtt_ms) {
	const u32 eleven_rtt = rtt_ms * rtt_high_factor;

	if (eleven_rtt < rtt_highvaluemarker_floor) {
		return rtt_highvaluemarker_floor;
	}

	if (eleven_rtt >= rtt_highvaluemarker_ceiling) {
		return rtt_highvaluemarker_ceiling;
	}

	return eleven_rtt;

}


static inline u32 get_rtt_low_watermarker(u32 rtt_ms) {

	const u32 adjust_rtt = rtt_ms;

        if (adjust_rtt < rtt_lowvaluemarker_floor) {
		return rtt_lowvaluemarker_floor;
	}

	if (adjust_rtt >= rtt_lowvaluemarker_ceiling) {
		return rtt_lowvaluemarker_ceiling;
	}

	return adjust_rtt;
}

static inline int get_maxcwnd_rtt_slope(const int rtt_highvaluemarker, const int rtt_lowvaluemarker)
{
	const int ydiff = cwnd_lowbound - cwnd_highbound;
	const int xdiff = rtt_highvaluemarker - rtt_lowvaluemarker;

        if ((cwnd_highbound <= cwnd_lowbound)
	    || (rtt_highvaluemarker <= rtt_lowvaluemarker))
		return 0;

	return (ydiff / xdiff);
}

static inline u32 adjust_cwnd_inbound_sgmnts(struct tcp_sock *tp) {
	u32 snd_cwnd_clamp = tp->snd_cwnd_clamp; /* current snd_cwnd_clamp */
	const u32 mss_cache = tp->mss_cache;
	u32 snd_cwnd_clamp_bytes = snd_cwnd_clamp * mss_cache;

	/* advertised window size from remote peer in bytes */
	const u32 snd_wnd = tp->snd_wnd;
	/* estimate the remote's peer's rwin = adv_win + packets_in_flight */
	const u32 estimated_rwin = snd_wnd + tp->packets_out * tp->mss_cache;
	const u32 reduced_rwnd = (estimated_rwin * cwnd_safe_factor) / 100;

	if (reduced_rwnd < snd_cwnd_clamp_bytes) {
		snd_cwnd_clamp_bytes = max(reduced_rwnd, init_cwnd * mss_cache);
		snd_cwnd_clamp = (snd_cwnd_clamp_bytes / mss_cache);
	}

#ifdef TCP_WRC_DEBUG
	pr_info("%s: line %d, tp: %p, snd_wnd: %u, maxwindow: %u, snd_cwnd: %u, snd_cwnd_clamp(current): %u, "\
		"snd_cwnd_cnt: %u, pkts_inflight: %u, mss_cache: %u "\
		"estimated_rwin:%u reduced_rwin: %u snd_cwnd_clamp_bytes: %u.\n",
		__FUNCTION__, __LINE__,
		tp, tp->snd_wnd, tp->max_window, tp->snd_cwnd, tp->snd_cwnd_clamp,
		tp->snd_cwnd_cnt, tp->packets_out, tp->mss_cache,
		estimated_rwin, reduced_rwnd, snd_cwnd_clamp_bytes);
#endif
	return snd_cwnd_clamp;
}


static inline void tcp_rbc_calculate_boundary(struct rbctcp *ca, const u32 rtt_ms)
{
	ca->rtt_highvaluemarker = get_rtt_high_watermarker(rtt_ms);
	ca->rtt_lowvaluemarker = get_rtt_low_watermarker(rtt_ms);
	ca->cwndslope = get_maxcwnd_rtt_slope(ca->rtt_highvaluemarker, ca->rtt_lowvaluemarker);
	return;
}


static void tcp_rbc_pkts_acked(struct sock *sk, const struct ack_sample *sample)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct rbctcp *ca = inet_csk_ca(sk);
	u32 srtt_crnt = 0;
	s32 rtt_ms = sample->rtt_us / 1000;
	u32 srtt_prev = ca->srtt;

	if (sample->rtt_us <= 0 ) {
		//rtt is not valid
		goto debug_info;
	}

	ca->rtt_cnt = ca->rtt_cnt + 1;

	if (unlikely(ca->rtt_cnt <= min_rtt_check_threshold)) {
		int flag = 0;
		if (unlikely(ca->init_rtt == 0)) {
			// initial rtt measured.
			ca->init_rtt = ca->srtt = ca->min_rtt = rtt_ms;
			srtt_prev = rtt_ms;
			flag = 1;
		} else if (unlikely(rtt_ms < ca->min_rtt)) {
			ca->min_rtt = rtt_ms;
			flag = 1;
		}

		if (unlikely(flag)) {
			// init RTT or smaller min_rtt detected.
			tcp_rbc_calculate_boundary(ca, rtt_ms);
		}

		if (!quiet && flag) {
			pr_info("sk %p detect min rtt %u\n", sk, ca->min_rtt);
			print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
		}
	}

	srtt_crnt = tcp_rbc_lpf_srtt(ca, rtt_ms, srtt_prev);

	if (srtt_crnt > 0)
		ca->srtt = srtt_crnt;
	else
		ca->srtt = rtt_ms;

	tp->snd_cwnd_clamp =
		get_cwnd_inbound_sgmnts(ca, ca->srtt, tp->mss_cache);

	tp->snd_cwnd_clamp = adjust_cwnd_inbound_sgmnts(tp);

debug_info:

	if (!quiet) {
		print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
	}
}

static void tcp_rbc_event(struct sock *sk, enum tcp_ca_event event)
{
	struct tcp_sock *tp = tcp_sk(sk);

	switch (event) {
	case CA_EVENT_LOSS:
		if (!quiet) {
			printk(KERN_INFO "RBC TCP LOSS EVENT send window: %d\n",
			       tp->snd_cwnd);
			print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
		}
		tp->snd_ssthresh = TCP_INFINITE_SSTHRESH;
		tp->snd_cwnd = init_cwnd;
		break;

	case CA_EVENT_COMPLETE_CWR:
	{
		if (tp->snd_ssthresh < TCP_INFINITE_SSTHRESH) {
			const struct rbctcp *ca = inet_csk_ca(sk);
			u32 rtt_ms = ca->srtt;
			tp->snd_cwnd = tp->snd_ssthresh;

			if (rtt_ms > ca_enter_threshold) {
				if (!quiet) {
					pr_warn("rtt %u is to large on sk:%p, entering ca mode after\n", rtt_ms, sk);
				}
			} else {
				tp->snd_ssthresh = TCP_INFINITE_SSTHRESH;
				if (!quiet) {
					pr_warn("sk:%p, entering slow start\n", sk);
				}
			}
		}

		if (!quiet) {
			pr_info("RBC TCP CWR EVENT send window: %u\n",
			       tp->snd_cwnd);
			print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
		}
		break;
	}
	default:
	{
		if (!quiet) {
			pr_info("RBC TCP EVENT received for sk 0x%p send window: %u when receiving event: %u\n",
			       sk, tp->snd_cwnd, event);
			print_tcp_sock_hdr(sk, __FUNCTION__, __LINE__);
		}
		break;
	}
	}

}

static struct tcp_congestion_ops tcp_rbc __read_mostly = {
	.init = tcp_rbc_init,
	.ssthresh = tcp_rbc_ssthresh,
	.cong_avoid = tcp_rbc_cong_avoid,
	.pkts_acked = tcp_rbc_pkts_acked,
	.cwnd_event = tcp_rbc_event,
	.undo_cwnd = tcp_reno_undo_cwnd,
	.owner = THIS_MODULE,
	.name = "satwrc3"
};

static int __init tcp_rbc_register(void)
{
	BUILD_BUG_ON(sizeof(struct rbctcp) > ICSK_CA_PRIV_SIZE);
	pr_info("TCP WRC version [%s] Register. struct tcp_sock size: %lu Bytes.\n",
		WRC_VERSION, sizeof(struct tcp_sock));
	return tcp_register_congestion_control(&tcp_rbc);
}

static void __exit tcp_rbc_unregister(void)
{
	pr_info("TCP WRC vesion [%s] UnRegister\n", WRC_VERSION);
	tcp_unregister_congestion_control(&tcp_rbc);
}

module_init(tcp_rbc_register);
module_exit(tcp_rbc_unregister);

MODULE_AUTHOR("SAT WRC/viasat");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTT based control TCP");
MODULE_VERSION(WRC_VERSION);
