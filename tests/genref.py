for i in range(20) :
  idx = str(i)
  if i < 10:
    idx = '0' + idx
  f = open('test' + idx + '.c', 'r')
  out = open('ref' + idx + '.txt', 'w')
  for line in f.readlines() :
    if line[0:2] == '//':
      out.write(line.strip(" /"))
  f.close()
  out.close()
      
