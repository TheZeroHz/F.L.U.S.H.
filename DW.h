#ifndef DW_H
#define DW_H

#include <Arduino.h>
#include <vector>
#define BEFORE_POOL 1
#define POOLING     2
#define END_POOL  3

class DW {
private:
    int PoolState = END_POOL;
    int w1 = 0, w2 = 0, dw = 0;
    bool dwValid = false;

public:
    void update(int weight, double zscore) {
        switch (PoolState) {
            case END_POOL:
                if (zscore == 0) {
                    w1 = weight;
                    PoolState = BEFORE_POOL;
                    dwValid = false;
                }
                break;

            case BEFORE_POOL:
                if (zscore != 0) {
                    PoolState = POOLING;
                }
                break;

            case POOLING:
                if (zscore == 0) {
                    w2 = weight;
                    dw = w2 - w1;
                    dwValid = true;
                    PoolState = END_POOL;
                }
                break;
        }
    }

    int getPoolState() const {
        return PoolState;
    }

    bool isDWReady() const {
        return dwValid;
    }

    int getDW() const {
        return dwValid ? dw : 0;  // Or throw an error / return sentinel if not ready
    }
};

#endif // DW_H
