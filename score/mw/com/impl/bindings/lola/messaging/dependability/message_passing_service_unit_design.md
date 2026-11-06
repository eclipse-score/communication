# LoLa Message Passing Service Unit Design

## `lola::MessagePassingService` is a Smart Proxy

All messaging interactions between the LoLa proxy/skeleton instances of two different LoLa enabled processes are routed
through their single/per process `lola::MessagePassingService` instances:

![LoLa Message Passing](./Lola_Messaging.svg)

As such a smart-proxy, the `lola::MessagePassingService` will not blindly route messages from proxies/skeletons from one
LoLa process to proxies/skeletons at the other side one-to-one!

Example: If there are two proxy instances within LoLa Process 1, which want to get notified about an update of the very
same event, which is provided by a skeleton instance in LoLa Process 2, then the `lola::MessagePassingService` in Process
1 is smart enough, not to send this `RegisterEventNotifierMsg` twice to the `lola::MessagePassingService` in Process 2!

**So the general idea is:**
Since we have made the decision, to route the entire message-passing of a LoLa enabled process over one (or two in case
of ASIL) messaging channel to get more flexibility/control over the amount of receiver/listener threads to be used, we
also shall take a further benefit!
That is: This central entity (`lola::MessagePassingService`), which cares for the routing, has total overview of the
messaging requirements of its local proxy/skeleton instances and should remove all potential redundancies and thereby
**minimizing the IPC** over `score::message_passing` to a bare minimum! Therefore, the notion of **Smart Proxy**.

Technically `lola::MessagePassingService` is a combination of a smart proxy and reverse-proxy. But since we have so many
proxies within our `score::mw` implementation, we did not name it as such to avoid further confusion. :)

## Setting up message passing infrastructure

The `lola::MessagePassingServiceInstance`s used by `lola::MessagePassingService` are created/set up during
initialization of our `mw::com` runtime. Depending on the `mw::com` deployment info for the
executable/process, it will be decided, whether only QM (ASIL-QM) or QM and ASIL (ASIL-B) instances are needed.

<img alt="LOLA_MSG_PASS_INITIALIZE_SEQ" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/messaging/dependability/lola_msg_pass_initialize_seq.puml">

## Event Update Notification

Event Notification is a good showcase for the "smart" behavior of `lola::MessagePassingService` as already mentioned (see
example above). Here event notification registrations are aggregated and related messages are only sent once:

<img alt="LOLA_MSG_PASS_EVENT_NOTIFY" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/messaging/dependability/lola_msg_pass_event_notify.puml">
