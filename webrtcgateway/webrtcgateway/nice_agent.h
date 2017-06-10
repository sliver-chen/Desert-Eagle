#ifndef _WXH_MY_NICE_H_
#define _WXH_MY_NICE_H_

#include <iostream>

#include "offer_answer_server.h"

#include <agent.h>
#include <gio/gnetworking.h>

#pragma comment(lib, "../libnice/nice/nice.lib")

#pragma comment(lib, "../glib/lib/gio-2.0.lib")
#pragma comment(lib, "../glib/lib/glib-2.0.lib")
#pragma comment(lib, "../glib/lib/gmodule-2.0.lib")
#pragma comment(lib, "../glib/lib/gobject-2.0.lib")
#pragma comment(lib, "../glib/lib/gthread-2.0.lib")

static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};

void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);
void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data);
void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data);
void cb_new_selected_pair_full(NiceAgent* agent, guint stream_id,guint component_id, NiceCandidate *lcandidate, NiceCandidate* rcandidate, gpointer user_data);
class nice_agent
{
public:
    explicit nice_agent(websocket_server* pserver, connection_hdl hdl, gboolean controlling, gchar* stun_addr = NULL, guint stun_port= 0)
    {
        pserver_ = pserver;
        hdl_ = hdl;
        agent = nullptr;        
        agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
        
        if (stun_addr)
        {
            g_object_set(agent, "stun-server", stun_addr, NULL);
            g_object_set(agent, "stun-server-port", stun_port, NULL);
        }
        g_object_set(agent, "upnp", false, NULL);
        g_object_set(agent, "ice-tcp", false, NULL);
        g_object_set(agent, "controlling-mode", controlling, NULL);        
        

        NiceAddress addr_local;
        nice_address_init (&addr_local);
        nice_address_set_from_string (&addr_local, "172.16.64.92");
        nice_agent_add_local_address (agent, &addr_local);

        g_signal_connect(agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), this);
        g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), this);
        g_signal_connect (agent, "new-selected-pair-full",G_CALLBACK (cb_new_selected_pair_full), this);
    }
    ~nice_agent()
    {
        if (agent)
        {
            g_object_unref(agent);            
        }         
    }
    int32_t add_stream(char* szstreamname, uint32_t componentnum)
    {
        guint stream_id = nice_agent_add_stream(agent, componentnum);
        if (stream_id == 0)
        {
            std::cout << "Failed to add stream" << std::endl;
        }
        else
        {
            nice_agent_set_stream_name (agent, stream_id, szstreamname);
            mapstream_componet[stream_id] = componentnum;
        }
        return stream_id;
    }
    bool start_gather(int32_t streamid)
    {
        // Attach to the component to receive the data
        // Without this call, candidates cannot be gathered
        std::map < int32_t, uint32_t>::iterator iter = mapstream_componet.find(streamid);
        if (mapstream_componet.end() == iter)
        {
            return false;
        }
        uint32_t ncomponet = iter->second;
        for (int inx = 1; inx <= ncomponet; inx++)
        {
            nice_agent_attach_recv(agent, streamid, inx, g_main_loop_get_context (gloop), cb_nice_recv, this);
        }
        if (!nice_agent_gather_candidates(agent, streamid))
        {
            g_error("Failed to start candidate gathering");
            return false;
        }
        return true;
    }
    void candidate_gathering_done(int32_t stream_id)
    {
        /*gchar* sdp = nice_agent_generate_local_sdp (agent);
        printf("Generated SDP from agent :\n%s\n\n", sdp);
        printf("Copy the following line to remote client:\n");
        gchar* sdp64 = g_base64_encode ((const guchar *)sdp, strlen (sdp));
        printf("\n  %s\n", sdp64);
        g_free (sdp);
        g_free (sdp64);*/

        gchar *local_ufrag = NULL;
        gchar *local_password = NULL;
        gchar ipaddr[INET6_ADDRSTRLEN];
        GSList *cands = NULL;


        if (!nice_agent_get_local_credentials(agent, stream_id,&local_ufrag, &local_password)) {return;}

        cands = nice_agent_get_local_candidates(agent, stream_id, 1);
        printf("%s %s", local_ufrag, local_password);

        NiceCandidate *c = (NiceCandidate *)g_slist_nth(cands, 0)->data;

        nice_address_to_string(&c->addr, ipaddr);

        // (foundation),(prio),(addr),(port),(type)
        printf(" %s,%u,%s,%u,%s\r\n",
            c->foundation,
            c->priority,
            ipaddr,
            nice_address_get_port(&c->addr),
            candidate_type_name[c->type]);



        char szsdp[1024*10] = {0}; 
        sprintf(szsdp, "v=0\r\no=- 1495799811084970 1495799811084970 IN IP4 172.16.64.92\r\ns=Streaming Test\r\nt=0 0\r\na=group:BUNDLE audio\r\na=msid-semantic: WMS janus\r\nm=audio 1 RTP/SAVPF 0\r\nc=IN IP4 172.16.64.92\r\na=mid:audio\r\na=sendonly\r\na=rtcp-mux\r\n"
            "a=ice-ufrag:%s\r\n"
            "a=ice-pwd:%s\r\na=ice-options:trickle\r\na=fingerprint:sha-256 D2:B9:31:8F:DF:24:D8:0E:ED:D2:EF:25:9E:AF:6F:B8:34:AE:53:9C:E6:F3:8F:F2:64:15:FA:E8:7F:53:2D:38\r\na=setup:actpass\r\na=connection:new\r\na=rtpmap:0 PCMU/8000\r\n"
            "a=ssrc:-537150489 cname:janusaudio\r\n"
            "a=ssrc:-537150489 msid:janus janusa0\r\n"
            "a=ssrc:-537150489 mslabel:janus\r\n"
            "a=ssrc:-537150489 label:janusa0\r\n"
            "a=candidate:%s 1 udp %u 172.16.64.92 %d typ host\r\n", 
            local_ufrag, local_password, c->foundation, c->priority, nice_address_get_port(&c->addr),
            candidate_type_name[c->type]
        );
        pserver_->send(hdl_, szsdp, websocketpp::frame::opcode::text);
        printf("\r\n%s\r\n", szsdp);

        if (local_ufrag)
            g_free(local_ufrag);
        if (local_password)
            g_free(local_password);
        if (cands)
            g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);
    }
    bool set_remote_sdp(const char* sdp)
    {
        remote_sdp = sdp;                
        return true;
    }
    bool set_remote_candidate(const char* szcandidate)
    {
        std::string szsdp = remote_sdp +"a="+ szcandidate + "\r\n";
        gchar* ufrag = NULL;
        gchar* pwd = NULL;
        GSList * plist = nice_agent_parse_remote_stream_sdp (agent, 1, szsdp.c_str(), &ufrag, &pwd);
        if (ufrag && pwd && g_slist_length(plist) > 0)
        {
            ufrag[strlen(ufrag)-1] = 0;
            pwd[strlen(pwd)-1] = 0;

            NiceCandidate* c = (NiceCandidate*)g_slist_nth(plist, 0)->data; 

            if (!nice_agent_set_remote_credentials(agent, 1, ufrag, pwd)) 
            {
                g_message("failed to set remote credentials");
                return false;
            }

            // Note: this will trigger the start of negotiation.

            if (nice_agent_set_remote_candidates(agent, 1, 1, plist) < 1) 
            {
                g_message("failed to set remote candidates");
                return false;
            }
            g_free(ufrag);
            g_free(pwd);
            //g_slist_free(plist);
            g_slist_free_full(plist, (GDestroyNotify)&nice_candidate_free);
        }
    }
    void component_state_changed(int32_t streamid, uint32_t componentid, guint state)
    {
        std::cout << this <<" state changed " << streamid << ":"<< componentid << " " << nice_component_state_to_string((NiceComponentState )state) 
                     << state << std::endl;
        if (state == NICE_COMPONENT_STATE_READY) 
        {

        }
    }

    void new_selected_pair_full(guint stream_id,guint component_id, NiceCandidate *lcandidate, NiceCandidate* rcandidate)
    {
        std::cout << this << "Component is ready enough, starting DTLS handshake...\n";

    }

    void nice_recv_data(int32_t streamid, uint32_t componentid, guint len, gchar *buf)
    {
        std::cout << this << " recv data from stream: " << streamid << " componetid: " << componentid << " len: " << len << "first data :"<< (int)buf[0] <<std::endl;

    }

private:
    NiceAgent *agent;
    std::map < int32_t, uint32_t> mapstream_componet;
    websocket_server* pserver_;
    connection_hdl hdl_;
    std::string remote_sdp;
    static GMainLoop *gloop;
    static GThread *gloopthread;
public:
    static void init()
    {
        g_networking_init();
        gloop = g_main_loop_new(NULL, FALSE);
        gloopthread = g_thread_new("loop thread", &loop_thread, gloop);
    }
    static void release()
    {
        g_main_loop_quit (gloop);
        g_thread_join (gloopthread);
        g_main_loop_unref(gloop);
    }
    static void* loop_thread(void *data)
    {
        GMainLoop* ploop = (GMainLoop*)data;
        std::cout << "loop thread going..." << std::endl;
        g_main_loop_run(ploop);
        std::cout << "loop thread quit..." << std::endl;
        return 0;
    }
};



#endif