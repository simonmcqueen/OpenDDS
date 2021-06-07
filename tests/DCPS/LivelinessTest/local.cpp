#include "common.h"
#include "Writer.h"
#include "DataReaderListener.h"

#include "../common/TestException.h"
#include "tests/DCPS/FooType4/FooDefTypeSupportImpl.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/StaticIncludes.h"
#include "dds/DCPS/WaitSet.h"
#if defined ACE_AS_STATIC_LIBS && !defined OPENDDS_SAFETY_PROFILE
#include "dds/DCPS/transport/udp/Udp.h"
#endif

#include "dds/DdsDcpsSubscriptionC.h"

#include "ace/Arg_Shifter.h"
#include "ace/Reactor.h"
#include "ace/OS_NS_unistd.h"

class ReactorCtrl : public ACE_Event_Handler
{
public:
  ReactorCtrl() : cond_(lock_) {}

  int handle_timeout (const ACE_Time_Value &tv,
                      const void *arg)
  {
    ACE_UNUSED_ARG(tv);
    ACE_UNUSED_ARG(arg);

    // it appears that you must have the lock before waiting or signaling on Win32
    ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                      guard,
                      this->lock_,
                      -1);

    return cond_.wait();
  }

private:
  ACE_Recursive_Thread_Mutex lock_;
  ACE_Condition<ACE_Recursive_Thread_Mutex> cond_;
} ;


/// parse the command line arguments
int parse_args (int argc, ACE_TCHAR *argv[])
{
  u_long mask =  ACE_LOG_MSG->priority_mask(ACE_Log_Msg::PROCESS) ;
  ACE_LOG_MSG->priority_mask(mask | LM_TRACE | LM_DEBUG, ACE_Log_Msg::PROCESS) ;
  ACE_Arg_Shifter arg_shifter (argc, argv);

  while (arg_shifter.is_anything_left ())
  {
    // options:
    //  -i num_ops_per_thread       defaults to 1
    //  -l num_unlively_periods     defaults to 10
    //  -w num_datawriters          defaults to 1
    //  -n max_samples_per_instance defaults to INFINITE
    //  -d history.depth            defaults to 1
    //  -z                          verbose transport debug
    //  -T                          prefix for temporary files

    const ACE_TCHAR *currentArg = 0;

    if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-i"))) != 0)
    {
      num_ops_per_thread = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-l"))) != 0)
    {
      num_unlively_periods = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-n"))) != 0)
    {
      max_samples_per_instance = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-d"))) != 0)
    {
      history_depth = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp(ACE_TEXT("-z")) == 0)
    {
      TURN_ON_VERBOSE_DEBUG;
      arg_shifter.consume_arg();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-T"))) == 0)
    {
      temp_file_prefix = currentArg;
      arg_shifter.consume_arg();
    }
    else
    {
      arg_shifter.ignore_arg ();
    }
  }
  // Indicates successful parsing of the command line
  return 0;
}


int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{

  OPENDDS_STRING transport("rtps_udp");


  OPENDDS_STRING config_1("dds4ccm_");
  config_1 += transport + "_1";

  OPENDDS_STRING instance_1("the_");
  instance_1 += transport + "_transport_1";

  OPENDDS_STRING config_2("dds4ccm_");
  config_2 += transport + "_2";

  OPENDDS_STRING instance_2("the_");
  instance_2 += transport + "_transport_2";

  int status = 0;

  try {
    ACE_DEBUG((LM_INFO,"(%P|%t) %T publisher main\n"));

    ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);

    OpenDDS::DCPS::TransportConfig_rch config =
      OpenDDS::DCPS::TransportRegistry::instance()->get_config(config_1.c_str());

    if (config.is_nil())
      {
        config =
          OpenDDS::DCPS::TransportRegistry::instance()->create_config(config_1.c_str());
      }

    OpenDDS::DCPS::TransportInst_rch inst =
      OpenDDS::DCPS::TransportRegistry::instance()->get_inst(instance_1.c_str());

    if (inst.is_nil())
      {
        inst =
          OpenDDS::DCPS::TransportRegistry::instance()->create_inst(instance_1.c_str(),
                                                                    transport.c_str());

        config->instances_.push_back(inst);

        OpenDDS::DCPS::TransportRegistry::instance()->global_config(config);
      }

    // Create another transport instance for participant2 since RTPS transport instances
    // cannot be shared by domain participants.
    OpenDDS::DCPS::TransportConfig_rch config2 =
      OpenDDS::DCPS::TransportRegistry::instance()->get_config(config_2.c_str());

    if (config2.is_nil())
      {
        config2 =
          OpenDDS::DCPS::TransportRegistry::instance()->create_config(config_2.c_str());
      }

    OpenDDS::DCPS::TransportInst_rch inst2 =
      OpenDDS::DCPS::TransportRegistry::instance()->get_inst(instance_2.c_str());

    if (inst2.is_nil())
      {
        inst2 =
          OpenDDS::DCPS::TransportRegistry::instance()->create_inst(instance_2.c_str(),
                                                                    transport.c_str());
        config2->instances_.push_back(inst2);

      }

    // let the Service_Participant (in above line) strip out -DCPSxxx parameters
    // and then get application specific parameters.
    parse_args (argc, argv);

    ::Xyz::FooTypeSupport_var fts(new ::Xyz::FooTypeSupportImpl);
    ::Xyz::FooTypeSupport_var fts2(new ::Xyz::FooTypeSupportImpl);

    ::DDS::DomainParticipant_var dp =
      dpf->create_participant(MY_DOMAIN,
                              PARTICIPANT_QOS_DEFAULT,
                              ::DDS::DomainParticipantListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    ::DDS::DomainParticipant_var dp2 =
      dpf->create_participant(MY_DOMAIN,
                              PARTICIPANT_QOS_DEFAULT,
                              ::DDS::DomainParticipantListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    OpenDDS::DCPS::TransportConfig_rch cfg = TheTransportRegistry->get_config("rtps");
    if (!cfg.is_nil()) {
      TheTransportRegistry->bind_config(cfg, dp);
    }
    cfg = TheTransportRegistry->get_config("rtps2");
    if (!cfg.is_nil()) {
      TheTransportRegistry->bind_config(cfg, dp2);
    }

    if (CORBA::is_nil(dp.in()) || CORBA::is_nil(dp2.in()))
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) create_participant failed.\n")));
      return 1 ;
    }

    if (::DDS::RETCODE_OK != fts->register_type(dp.in (), MY_TYPE))
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT ("Failed to register the FooTypeSupport.")));
        return 1;
      }

    if (::DDS::RETCODE_OK != fts2->register_type(dp2.in (), MY_TYPE))
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT ("Failed to register the FooTypeSupport.")));
        return 1;
      }


    ::DDS::TopicQos topic_qos;
    dp->get_default_topic_qos(topic_qos);

    topic_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance ;

    topic_qos.history.depth = history_depth;

    ::DDS::Topic_var automatic_topic =
      dp->create_topic (AUTOMATIC_TOPIC,
                        MY_TYPE,
                        topic_qos,
                        ::DDS::TopicListener::_nil(),
                        ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    ::DDS::Topic_var manual_topic =
      dp->create_topic (MANUAL_TOPIC,
                        MY_TYPE,
                        topic_qos,
                        ::DDS::TopicListener::_nil(),
                        ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    ::DDS::Topic_var automatic_topic2 =
      dp2->create_topic (AUTOMATIC_TOPIC,
                        MY_TYPE,
                        topic_qos,
                        ::DDS::TopicListener::_nil(),
                        ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    ::DDS::Topic_var manual_topic2 =
      dp2->create_topic (MANUAL_TOPIC,
                        MY_TYPE,
                        topic_qos,
                        ::DDS::TopicListener::_nil(),
                        ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (CORBA::is_nil (automatic_topic.in ()) || CORBA::is_nil (manual_topic.in ()))
    {
      return 1 ;
    }

    // Create the publisher
    ::DDS::Publisher_var pub =
      dp->create_publisher(PUBLISHER_QOS_DEFAULT,
                            ::DDS::PublisherListener::_nil(),
                            ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (CORBA::is_nil (pub.in ()))
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                        ACE_TEXT("(%P|%t) create_publisher failed.\n")),
                        1);
    }

    // Create the datawriters
    ::DDS::DataWriterQos automatic_dw_qos;
    pub->get_default_datawriter_qos (automatic_dw_qos);
    automatic_dw_qos.history.depth = history_depth;
    automatic_dw_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance;
    automatic_dw_qos.liveliness.kind = ::DDS::AUTOMATIC_LIVELINESS_QOS;
    automatic_dw_qos.liveliness.lease_duration.sec = LEASE_DURATION_SEC;
    automatic_dw_qos.liveliness.lease_duration.nanosec = 0;

    ::DDS::DataWriterQos manual_dw_qos;
    pub->get_default_datawriter_qos (manual_dw_qos);
    manual_dw_qos.history.depth = history_depth;
    manual_dw_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance;
    manual_dw_qos.liveliness.kind = ::DDS::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
    manual_dw_qos.liveliness.lease_duration.sec = LEASE_DURATION_SEC;
    manual_dw_qos.liveliness.lease_duration.nanosec = 0;

    ::DDS::DataWriter_var dw_automatic =
      pub->create_datawriter(automatic_topic.in (),
                              automatic_dw_qos,
                              ::DDS::DataWriterListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    ::DDS::DataWriter_var dw_manual =
      pub->create_datawriter(manual_topic.in (),
                              manual_dw_qos,
                              ::DDS::DataWriterListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);


    if (CORBA::is_nil (dw_automatic.in ()) || CORBA::is_nil (dw_manual.in ()))
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) create_datawriter failed.\n")));
      return 1 ;
    }
    ::DDS::TopicDescription_var manual_description =
      dp->lookup_topicdescription(MANUAL_TOPIC);
    ::DDS::TopicDescription_var automatic_description2 =
      dp2->lookup_topicdescription(AUTOMATIC_TOPIC);
    ::DDS::TopicDescription_var manual_description2 =
      dp2->lookup_topicdescription(MANUAL_TOPIC);
    if (CORBA::is_nil (CORBA::is_nil (manual_description.in ()) || 
        automatic_description2.in ()) || CORBA::is_nil (manual_description2.in ()))
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT("(%P|%t) lookup_topicdescription failed.\n")),
                          1);
    }

    // Create the subscriber
    ::DDS::Subscriber_var remote_sub =
      dp2->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                            ::DDS::SubscriberListener::_nil(),
                            ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    ::DDS::Subscriber_var local_sub =
      dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                            ::DDS::SubscriberListener::_nil(),
                            ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (CORBA::is_nil(remote_sub.in()) || CORBA::is_nil(local_sub.in()))
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT("(%P|%t) create_subscriber failed.\n")),
                          1);
    }

    // Create the Datareaders
    ::DDS::DataReaderQos automatic_dr_qos;
    remote_sub->get_default_datareader_qos (automatic_dr_qos);
    automatic_dr_qos.history.depth = history_depth  ;
    automatic_dr_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance ;
    automatic_dr_qos.liveliness.kind = ::DDS::AUTOMATIC_LIVELINESS_QOS;
    automatic_dr_qos.liveliness.lease_duration.sec = LEASE_DURATION_SEC ;
    automatic_dr_qos.liveliness.lease_duration.nanosec = 0 ;

    ::DDS::DataReaderQos remote_manual_dr_qos;
    remote_sub->get_default_datareader_qos (remote_manual_dr_qos);
    remote_manual_dr_qos.history.depth = history_depth  ;
    remote_manual_dr_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance ;
    remote_manual_dr_qos.liveliness.kind = ::DDS::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
    remote_manual_dr_qos.liveliness.lease_duration.sec = LEASE_DURATION_SEC ;
    remote_manual_dr_qos.liveliness.lease_duration.nanosec = 0 ;

    ::DDS::DataReaderQos local_manual_dr_qos;
    local_sub->get_default_datareader_qos (local_manual_dr_qos);
    local_manual_dr_qos.history.depth = history_depth  ;
    local_manual_dr_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance ;
    local_manual_dr_qos.liveliness.kind = ::DDS::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
    local_manual_dr_qos.liveliness.lease_duration.sec = LEASE_DURATION_SEC ;
    local_manual_dr_qos.liveliness.lease_duration.nanosec = 0 ;

    ::DDS::DataReaderListener_var drl (new DataReaderListenerImpl("RemoteAutomaticReader"));
    DataReaderListenerImpl* drl_servant =
      dynamic_cast<DataReaderListenerImpl*>(drl.in());
    ::DDS::DataReaderListener_var drl2 (new DataReaderListenerImpl("RemoteManualReader"));
    DataReaderListenerImpl* drl_servant2 =
      dynamic_cast<DataReaderListenerImpl*>(drl2.in());
    ::DDS::DataReaderListener_var drl3 (new DataReaderListenerImpl("LocalManualReader"));
    DataReaderListenerImpl* drl_servant3 =
      dynamic_cast<DataReaderListenerImpl*>(drl3.in());

    if (!drl_servant || !drl_servant2 || !drl_servant3) {
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("%N:%l main()")
        ACE_TEXT(" ERROR: drl_servant is nil (dynamic_cast failed)!\n")), -1);
    }

    ::DDS::DataReader_var automatic_dr ;

    automatic_dr = remote_sub->create_datareader(automatic_description2.in (),
                                automatic_dr_qos,
                                drl.in (),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    ::DDS::DataReader_var remote_manual_dr ;

    remote_manual_dr = remote_sub->create_datareader(manual_description2.in (),
                                remote_manual_dr_qos,
                                drl2.in (),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::StatusCondition_var condition = remote_manual_dr->get_statuscondition();
    condition->set_enabled_statuses(DDS::SUBSCRIPTION_MATCHED_STATUS);
    DDS::WaitSet_var ws = new DDS::WaitSet;
    ws->attach_condition(condition);        
    while (true) {
      DDS::SubscriptionMatchedStatus matches;
      if (remote_manual_dr->get_subscription_matched_status(matches) != ::DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("ERROR: %N:%l: main() -")
                          ACE_TEXT(" get_subscription_matched_status failed!\n")),
                         1);
      }
      if (matches.current_count >= 1) {
        break;
      }
      DDS::ConditionSeq conditions;
      DDS::Duration_t timeout = { 60, 0 };
      if (ws->wait(conditions, timeout) != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("ERROR: %N:%l: main() -")
                          ACE_TEXT(" wait failed!\n")),
                         1);
      }
    }
    ws->detach_condition(condition);

    // I need this sleep to ensure correct values of no_writers_generation_count for
    // the rtps transport between different participants.
    sleep(1);

    ::DDS::DataReader_var local_manual_dr ;

    local_manual_dr = local_sub->create_datareader(manual_description.in (),
                                local_manual_dr_qos,
                                drl3.in (),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    
    if (CORBA::is_nil (automatic_dr.in ()) || CORBA::is_nil (remote_manual_dr.in ()) ||
        CORBA::is_nil (local_manual_dr.in ()))
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) create_datawriter failed.\n")));
      return 1 ;
    }
    // send an automatic message to show manual readers are not
    // notified of liveliness.
    ::Xyz::Foo foo;
    foo.x = -1;
    foo.y = -1;
    foo.key = 101010;
    ::Xyz::FooDataWriter_var foo_dw
      = ::Xyz::FooDataWriter::_narrow(dw_automatic.in());
    ::DDS::InstanceHandle_t handle = foo_dw->register_instance(foo);
    foo_dw->write(foo,
                  handle);

    Writer* writer = new Writer(dw_manual.in(),
                                1,
                                num_ops_per_thread);
    int lcc_local = 0;
    int lcc_remote = 0;
    //we want to only publish after the reader loses liveliness from the writer
    //the follows the pattern of an up and a down, so there should be 2 liveliness
    //changes per call to run_test
    for (int i = 0 ; i < num_unlively_periods + 1 ; ++i) {
      lcc_local = drl_servant3->liveliness_changed_count();
      lcc_remote = drl_servant2->liveliness_changed_count();
      while(lcc_local != 2 * i || lcc_remote != 2 * i) {
        ACE_OS::sleep(ACE_Time_Value(0, 250000));
        lcc_local = drl_servant3->liveliness_changed_count();
        lcc_remote = drl_servant2->liveliness_changed_count();
      }
      ACE_DEBUG ((LM_DEBUG,
                  ACE_TEXT("(%P|%t) Running Write: remote: %d local: %d \n"), lcc_remote, lcc_local));
      writer->run_test (i);
    }

    while(lcc_local != 2 * (num_unlively_periods + 1) ||
          lcc_remote != 2 * (num_unlively_periods + 1)) {
      ACE_OS::sleep(ACE_Time_Value(0, 250000));
      lcc_local = drl_servant3->liveliness_changed_count();
      lcc_remote = drl_servant2->liveliness_changed_count();
    }

    // Clean up publisher objects
    pub->delete_contained_entities() ;
    delete writer;
    dp->delete_publisher(pub.in ());

    while(lcc_local != 2 * (num_unlively_periods + 2) + 1 &&
          lcc_remote != 2 * (num_unlively_periods + 2) + 1){
      ACE_OS::sleep(ACE_Time_Value(0, 250000));
      lcc_local = drl_servant3->liveliness_changed_count();
      lcc_remote = drl_servant2->liveliness_changed_count();
    }
    
    // Determine the test status at this point.
    
    ACE_OS::fprintf (stderr, "**********\n") ;
    ACE_OS::fprintf (stderr, "drl_servant->liveliness_changed_count() = %d\n",
                    drl_servant->liveliness_changed_count()) ;
    ACE_OS::fprintf (stderr, "drl_servant->no_writers_generation_count() = %d\n",
                    drl_servant->no_writers_generation_count()) ;
    ACE_OS::fprintf (stderr, "********** use_take=%d\n", use_take) ;

    //automatic should stay alive due to reactor and 
    //therefore go up at start then come down at end
    if( drl_servant->liveliness_changed_count() != 3) {
      status = 1;
      // Some error condition.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed first condition.\n")
      ));

    } else if( drl_servant->verify_last_liveliness_status () == false) {
      status = 1;
      // Some other error condition.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed second condition.\n")
      ));

    } else if( drl_servant->no_writers_generation_count() != 0) {
      status = 1;
      // Yet another error condition.

      // Using take will remove the instance and instance state will be
      // reset for any subsequent samples sent.  Since there are no
      // more samples sent, the information available from the listener
      // retains that from the last read sample rather than the reset
      // value for an (as yet unreceived) next sample.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed third condition.\n")
      ));
    }
    ACE_OS::fprintf (stderr, "**********\n") ;
    ACE_OS::fprintf (stderr, "drl_servant2->liveliness_changed_count() = %d\n",
                    drl_servant2->liveliness_changed_count()) ;
    ACE_OS::fprintf (stderr, "drl_servant2->no_writers_generation_count() = %d\n",
                    drl_servant2->no_writers_generation_count()) ;
    ACE_OS::fprintf (stderr, "********** use_take=%d\n", use_take) ;

    if( drl_servant2->liveliness_changed_count() < 2 + 2) {
      status = 1;
      // Some error condition.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed first condition.\n")
      ));

    } else if( drl_servant2->verify_last_liveliness_status () == false) {
      status = 1;
      // Some other error condition.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed second condition.\n")
      ));

    } else if( drl_servant2->no_writers_generation_count() != num_unlively_periods) {
      status = 1;
      // Yet another error condition.

      // Using take will remove the instance and instance state will be
      // reset for any subsequent samples sent.  Since there are no
      // more samples sent, the information available from the listener
      // retains that from the last read sample rather than the reset
      // value for an (as yet unreceived) next sample.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed third condition.\n")
      ));
    }
    ACE_OS::fprintf (stderr, "**********\n") ;
    ACE_OS::fprintf (stderr, "drl_servant3->liveliness_changed_count() = %d\n",
                    drl_servant3->liveliness_changed_count()) ;
    ACE_OS::fprintf (stderr, "drl_servant3->no_writers_generation_count() = %d\n",
                    drl_servant3->no_writers_generation_count()) ;
    ACE_OS::fprintf (stderr, "********** use_take=%d\n", use_take) ;

    if( drl_servant3->liveliness_changed_count() < 2 + 2) {
      status = 1;
      // Some error condition.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed first condition.\n")
      ));

    } else if( drl_servant3->verify_last_liveliness_status () == false) {
      status = 1;
      // Some other error condition.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed second condition.\n")
      ));

    } else if( drl_servant3->no_writers_generation_count() != num_unlively_periods) {
      status = 1;
      // Yet another error condition.

      // Using take will remove the instance and instance state will be
      // reset for any subsequent samples sent.  Since there are no
      // more samples sent, the information available from the listener
      // retains that from the last read sample rather than the reset
      // value for an (as yet unreceived) next sample.
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: subscriber - ")
        ACE_TEXT("test failed third condition.\n")
      ));
    }

      ACE_DEBUG((LM_DEBUG,ACE_TEXT("(%P|%t) %T publisher is finish - cleanup subscriber\n") ));
      local_sub->delete_contained_entities() ;
      remote_sub->delete_contained_entities() ;
      dp->delete_subscriber(local_sub.in ());
      dp->delete_subscriber(remote_sub.in ());
      dp->delete_topic(automatic_topic.in ());
      dp->delete_topic(manual_topic.in ());
      dpf->delete_participant(dp.in ());
      dpf->delete_participant(dp2.in ());
      TheServiceParticipant->shutdown ();
    }
  catch (const TestException&)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in main.cpp. ")));
      return 1;
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in main.cpp:");
      return 1;
    }

  return status;
}
