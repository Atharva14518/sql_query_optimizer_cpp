#include "stats.h"

using namespace sqlopt;

void StatsCatalog::load_defaults(){
    TableStats users; users.name="users"; users.row_count=100000; users.distinct_vals={{"id",100000},{"name",80000},{"age",80},{"orig",100000}}; users.indexes={{"id"},{"name"}};
    TableStats orders; orders.name="orders"; orders.row_count=500000; orders.distinct_vals={{"id",500000},{"order_id",500000},{"user_id",90000},{"status",5},{"order_date",365},{"order_amount",100000}}; orders.indexes={{"user_id","order_id"},{"status"}};
    TableStats products; products.name="products"; products.row_count=20000; products.distinct_vals={{"id",20000},{"name",18000},{"product_category",10}}; products.indexes={{"id"},{"name"}};
    TableStats customers; customers.name="customers"; customers.row_count=100000; customers.distinct_vals={{"customer_id",100000},{"customer_name",80000},{"customer_age",80}}; customers.indexes={{"customer_id"}};
    TableStats order_items; order_items.name="order_items"; order_items.row_count=1000000; order_items.distinct_vals={{"order_id",500000},{"product_id",20000}}; order_items.indexes={{"order_id"},{"product_id"}};
    tables[users.name]=users; tables[orders.name]=orders; tables[products.name]=products; tables[customers.name]=customers; tables[order_items.name]=order_items;
}
