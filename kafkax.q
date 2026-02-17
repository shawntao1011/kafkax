/ C funcs
.kfkx.consumer: `libkafkax_q 2:(`kfkx_initconsumer;1)
.kfkx.bind:     `libkafkax_q 2:(`kfkx_bind;4)
.kfkx.sub:      `libkafkax_q 2:(`kfkx_subscribe;2)
.kfkx.drain:    `libkafkax_q 2:(`kfkx_drain;2)

.kfkx.i: 0;
.kfkx.upd:{[tbl;data]  / data is qipc bytes (KG vector)
 .kfkx.i+:1;
 }

.kfkx.onfd:{[h]
  t:.kfkx.drain[h;4096];
  if[0=count t; :()];
  / route rows
  {[r]
    if[`data~r[`kind];  .kfkx.upd[r`tbl; r`data]];
    if[`data~r[`error]; show ("[kfkx] ", string r`topic, " ", r`err)];
  } each t;
 }

/ start
cfg:(`bootstrap.servers`group.id`auto.offset.reset`enable.auto.commit`decode_threads`raw_queue_size`evt_queue_size)!
     ("192.168.2.209:9092";"q-sub-demo";"earliest";1;4;8192;8192);

h:.kfkx.consumer cfg;

.kfkx.bind[h; `futu.orderbook.pb; "plugins/libkafkax_default_decoder.so"; `kafkax_default_decode];

.kfkx.sub[h; `futu.orderbook.pb];