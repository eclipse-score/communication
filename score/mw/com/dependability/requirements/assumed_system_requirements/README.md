# Assumptions for SEooC

The goal of the communication framework (`mw::com`) is to develop a Safety Element out of Context as per the ISO26262:
2018 Part 10
chapter 9. As per this, we have to state assumptions towards the system for which this SEooC is developed.

## Assumptions on the scope of the software component

> Following ISO26262:2018 Part 10 - Requirement 9.2.4.2

Given a system based on a POSIX based operating system, where multiple processes are running. These processes require
to exchange data with each other to execute their functionality.

Since not every process should come up with its own custom infrastructure on how to exchange data, it is the main purpose
of this software component to provide infrastructure for processes to exchange data in a safe and secure manner.

The assumed functional requirements are specified
within [assumed_functional_requirements.trlc](assumed_functional_requirements.trlc).

We assume further that the software component is part of the following layered architecture:

![](assumed_layered_software_architecture.drawio.svg)

Where the apps represent different process that utilize our `mw::com` component to exchange data with each other.

## Assumptions for the safety requirements

> Following ISO26262:2018 Part 10 - Requirement 9.2.4.3

In accordance with ISO26262:2018 Part 8 - Chapter 6.2, our assumed safety requirements, should be on the level of the
technical safety concept (ISO26262:2018 4-6). They are defined
here [assumed_technical_safety_requirements](./assumed_technical_safety_requirements.trlc).
