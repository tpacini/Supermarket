/* The director:
    - opens or closes registers (based on cashier's information)
        - the cashier periodically reports to the director the number of customers in the queue 
        - the director exploits the value S1 and S2 to make decisions 
            - S1=n, if there are more than n registers with max. one customer in the queue 
            - S2=m, open a register (if possible) if there is at least a register with at least m customers in the queue 
            - these parameters are choosen by the student
    - send a SIGQUIT or SIGHUP signal to the supermarket process if it receives one of those signals
        - SIGHUP, the supermarket closes the entrance, no more customers allowed, and wait that all the customers leave the supermarket 
        - SIGQUIT, the customers are let out from the supermarket
        - The director waits the termination of supermarket before terminate itself 
    - is notified by a customer with zero products to leave the supermarket
*/

#pragma once


void Director()
{

}


long to_long(char* to_convert);