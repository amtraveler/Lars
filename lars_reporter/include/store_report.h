#include "lars.pb.h"
#include <string>
#include "mysql.h"
#include "lars_reactor.h"

class StoreReport
{
public:
	StoreReport();

	void store(lars::ReportStatusRequest& req);
private:
	MYSQL _db_conn;
};

void *store_main(void*);
