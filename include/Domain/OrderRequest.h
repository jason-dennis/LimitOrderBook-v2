//
// Created by Ognean Jason Dennis on 20.07.2026.
//

#ifndef LIMITORDERBOOK_ORDERREQUEST_H
#define LIMITORDERBOOK_ORDERREQUEST_H
#include <string>

struct OrderRequest {
    float Price;
    int Quantity;
    std::string Type;
    std::string Symbol;
    std::string TIF;
    int TraderID;
    std::string Side;
};

#endif //LIMITORDERBOOK_ORDERREQUEST_H
