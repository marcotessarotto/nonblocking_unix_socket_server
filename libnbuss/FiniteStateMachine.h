#ifndef LIBNBUSS_FINITESTATEMACHINE_H_
#define LIBNBUSS_FINITESTATEMACHINE_H_

#include <iostream>
#include <string>
#include <functional>
#include <tuple>
#include <array>
#include <list>

template<typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e)
{
    return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

template <typename StateType, typename EventType>
class FiniteStateMachine {

	StateType state;

public:

	static void noop() {
		// do nothing
	}

	/// lambda type for action to do when there is a match on (state, event)
	using TFSMAction =  std::function<void(void)>;

	/// description of finite state machine
	using TFSMTuple = std::tuple<StateType , EventType, TFSMAction>;

	std::list<TFSMTuple> data;


	FiniteStateMachine(StateType initialState) : state(initialState) {}

	void setData(std::list<TFSMTuple> data) { this->data = data; }

	void setState(StateType newState) { state = newState; }

	void incomingEvent(EventType ev) {
		// https://stackoverflow.com/a/47832420/974287
		for (const auto& [state, _ev, lambda] : data) {
			//std::cout << state << " " << ev << std::endl;
		    if (this->state == state && _ev == ev) {
		    	std::cout << "match! "<< state << " " << ev << std::endl;

		    	lambda();

		    	return;
		    }
		}

		throw std::runtime_error("invalid (state, event)");
	}


	void printState() { std::cout << "current state is: " << state << std::endl; }

	static void call(StateType new_state, FiniteStateMachine * sm, TFSMAction func) {
		sm->state = new_state; func(sm);
	}

};

/*
example:


	enum class State {
		STATE0_IDLE,
		STATE1_HAVE_SENT_PUBK,
		STATE2_HAVE_RCV_UAKE_SEND_A,
		STATE3_HAVE_SENT_UAKE_SEND_B, // and then send test_request immediately
		STATE4_HANDSHAKE_COMPLETE,
		STATE5_FAILURE
	};

	const std::list<State> all_states = {
		State::STATE0_IDLE ,
		State::STATE1_HAVE_SENT_PUBK ,
		State::STATE2_HAVE_RCV_UAKE_SEND_A ,
		State::STATE3_HAVE_SENT_UAKE_SEND_B ,
		State::STATE4_HANDSHAKE_COMPLETE ,
		State::STATE5_FAILURE
	};

	enum class Event {
		EVENT_A_NEW_CONN, // there is a new connection, proceed (action: send our pubkey)
		EVENT_B_RCV_UAKE_SEND_A, // have received uake_send_a, switch to state2 and start processing uake_send_a
		EVENT_C_READY_UAKE_SEND_B, // uake_send_b is ready, now send it to remote peer; and then immediately afeter send test_request
		EVENT_D_RCV_TEST_RESPONSE, // evaluate incoming test response; if correct, goto state4 else goto state5
		EVENT_E,
	};
	const std::list<Event> all_events = {
		Event::EVENT_A_NEW_CONN,
		Event::EVENT_B_RCV_UAKE_SEND_A,
		Event::EVENT_C_READY_UAKE_SEND_B,
		Event::EVENT_D_RCV_TEST_RESPONSE,
		Event::EVENT_E,
	};



	using TMyFSM = FiniteStateMachine<State, Event>;


	TMyFSM fsm{State::STATE0_IDLE};

	std::list<TMyFSM::TFSMTuple> data = {
		std::make_tuple(State::STATE0_IDLE,
				Event::EVENT_A_NEW_CONN,
				[&fsm]() {
					// send puk
					fsm.setState(State::STATE1_HAVE_SENT_PUBK);
				}),

		std::make_tuple(State::STATE1_HAVE_SENT_PUBK,
				Event::EVENT_B_RCV_UAKE_SEND_A,
				[&fsm]() {
					// TODO: calc uake_send_b

				}),

		std::make_tuple(State::STATE2_HAVE_RCV_UAKE_SEND_A,
				Event::EVENT_B_RCV_UAKE_SEND_A,
				TMyFSM::noop
				),

	};

	fsm.setData(data);


	fsm.printState();

	fsm.incomingEvent(Event::EVENT_A_NEW_CONN);


	fsm.printState();



 */


#endif /* LIBNBUSS_FINITESTATEMACHINE_H_ */
