// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */



#ifndef CEPH_MDS_H
#define CEPH_MDS_H

#include "mdstypes.h"

#include "msg/Dispatcher.h"
#include "include/CompatSet.h"
#include "include/types.h"
#include "include/Context.h"
#include "common/DecayCounter.h"
#include "common/perf_counters.h"
#include "common/Mutex.h"
#include "common/Cond.h"
#include "common/Timer.h"
#include "common/LogClient.h"
#include "common/TrackedOp.h"
#include "common/Finisher.h"
#include "common/cmdparse.h"
#include "mgr/MgrClient.h"

#include "MDSRank.h"
#include "MDSMap.h"

#include "Beacon.h"


#define CEPH_MDS_PROTOCOL    29 /* cluster internal */

class MonClient;

class Server;
class Locker;
class MDCache;
class MDBalancer;
class MDSInternalContextBase;

class Messenger;
class Message;

class SnapServer;
class SnapClient;

class MDSTableServer;
class MDSTableClient;

class AuthAuthorizeHandlerRegistry;

class MDSDaemon : public Dispatcher, public md_config_obs_t {
 public:
  /* Global MDS lock: every time someone takes this, they must
   * also check the `stopping` flag.  If stopping is true, you
   * must either do nothing and immediately drop the lock, or
   * never drop the lock again (i.e. call respawn()) */
  Mutex        mds_lock;
  bool         stopping;

  SafeTimer    timer;

 protected:
  Beacon  beacon;

  AuthAuthorizeHandlerRegistry *authorize_handler_cluster_registry;
  AuthAuthorizeHandlerRegistry *authorize_handler_service_registry;

  std::string name;

  Messenger    *messenger;
  MonClient    *monc;
  MgrClient     mgrc;
  MDSMap       *mdsmap;
  LogClient    log_client;
  LogChannelRef clog;

  MDSRankDispatcher *mds_rank;

 public:
  MDSDaemon(const std::string &n, Messenger *m, MonClient *mc);
  ~MDSDaemon() override;
  int orig_argc;
  const char **orig_argv;

  // handle a signal (e.g., SIGTERM)
  void handle_signal(int signum);

  int init();

  /**
   * Hint at whether we were shutdown gracefully (i.e. we were only
   * in standby, or our rank was stopped).  Should be removed once
   * we handle shutdown properly (e.g. clear out all message queues)
   * such that deleting xlists doesn't assert.
   */
  bool is_clean_shutdown();

  // config observer bits
  const char** get_tracked_conf_keys() const override;
  void handle_conf_change(const struct md_config_t *conf,
				  const std::set <std::string> &changed) override;
 protected:
  // tick and other timer fun
  class C_MDS_Tick;
  C_MDS_Tick *tick_event;
  void     reset_tick();

  void wait_for_omap_osds();

 private:
  bool ms_dispatch(Message *m) override;
  bool ms_get_authorizer(int dest_type, AuthAuthorizer **authorizer, bool force_new) override;
  bool ms_verify_authorizer(Connection *con, int peer_type,
			       int protocol, bufferlist& authorizer_data, bufferlist& authorizer_reply,
			       bool& isvalid, CryptoKey& session_key) override;
  void ms_handle_accept(Connection *con) override;
  void ms_handle_connect(Connection *con) override;
  bool ms_handle_reset(Connection *con) override;
  void ms_handle_remote_reset(Connection *con) override;
  bool ms_handle_refused(Connection *con) override;

 protected:
  // admin socket handling
  friend class MDSSocketHook;
  class MDSSocketHook *asok_hook;
  void set_up_admin_socket();
  void clean_up_admin_socket();
  void check_ops_in_flight(); // send off any slow ops to monitor
  bool asok_command(string command, cmdmap_t& cmdmap, string format,
		    ostream& ss);

  void dump_status(Formatter *f);

  /**
   * Terminate this daemon process.
   *
   * This function will return, but once it does so the calling thread
   * must do no more work as all subsystems will have been shut down.
   */
  void suicide();

  /**
   * Start a new daemon process with the same command line parameters that
   * this process was run with, then terminate this process
   */
  void respawn();

  void tick();
  
  // messages
  bool _dispatch(Message *m, bool new_msg);

protected:
  bool handle_core_message(Message *m);
  
  // special message types
  friend class C_MDS_Send_Command_Reply;
  static void send_command_reply(MCommand *m, MDSRank* mds_rank, int r,
				 bufferlist outbl, const std::string& outs);
  int _handle_command(
      const cmdmap_t &cmdmap,
      MCommand *m,
      bufferlist *outbl,
      std::string *outs,
      Context **run_later,
      bool *need_reply);
  void handle_command(class MCommand *m);
  void handle_mds_map(class MMDSMap *m);
  void _handle_mds_map(MDSMap *oldmap);
};


#endif
