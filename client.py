from multiprocessing import Value
from threading import Timer
from utils import States
import multiprocessing
import random
import socket
import time
import utils

UDP_IP = "192.168.1.33"
UDP_PORT = 5005
MSS = 12 # maximum segment size

sock = socket.socket(socket.AF_INET,    # Internet
                     socket.SOCK_DGRAM) # UDP

# this timeout is important, particularly for connection 
# establishment success, but also as it directly influences 
# the time-to-resend, as acks are only assumed not received once 
# the timeout expires
sock.settimeout(3.0)
# sock.settimeout(0.2) # tested to work: as small as possible minimizes runtime, but too small causes unnecessary resends (e.g., due to delays); tcp timer length really based on RTT estimation?

def send_udp(message):
  sock.sendto(message, (UDP_IP, UDP_PORT))

def chunkstring(string, length):
    return (string[0+i:length+i] for i in range(0, len(string), length))

class Client:
  def __init__(self):
    self.client_state = States.CLOSED
    self.last_received_ack = 0
    self.last_received_seq_num = 0
    self.handshake()


  def handshake(self):
    while self.client_state != States.ESTABLISHED:
      if self.client_state == States.CLOSED:
        seq_num = utils.rand_int()
        self.next_seq_num = seq_num + 1
        syn_header = utils.Header(seq_num, 0, syn = 1, ack = 0, fin = 0)
        if utils.DEBUG:
          print("In Handshake - Closed - Sending ...\n")
          syn_header.__str__()
        send_udp(syn_header.bits())
        self.update_state(States.SYN_SENT)
      elif self.client_state == States.SYN_SENT:
        recv_data, addr = sock.recvfrom(1024)
        syn_ack_header = utils.bits_to_header(recv_data)
        if utils.DEBUG:
          print("In Handshake - SYN_SENT - Received ...")
          syn_ack_header.__str__()
        self.last_received_seq_num = syn_ack_header.seq_num
        if syn_ack_header.syn == 1 and syn_ack_header.ack == 1:
          ack_header = utils.Header( self.next_seq_num
                                   , self.last_received_seq_num + 1
                                   , syn = 0, ack = 1, fin = 0)
          self.next_seq_num += 1
          send_udp(ack_header.bits())
          self.update_state(States.ESTABLISHED)
      else:
        pass

  def terminate(self):
    while self.client_state != States.CLOSED:
      if self.client_state == States.ESTABLISHED:
        terminate_header = utils.Header(self.next_seq_num, self.last_received_seq_num + 1, syn = 0, ack = 1, fin = 1)
        self.next_seq_num += 1
        send_udp(terminate_header.bits())
        self.update_state(States.FIN_WAIT_1)
      elif self.client_state == States.FIN_WAIT_1:
        recv_data, addr = sock.recvfrom(1024)
        fin_ack_header = utils.bits_to_header(recv_data)
        self.last_received_seq_num = fin_ack_header.seq_num
        if fin_ack_header.ack == 1:
          self.update_state(States.FIN_WAIT_2)
      elif self.client_state == States.FIN_WAIT_2:
        recv_data, addr = sock.recvfrom(1024)
        fin_fin_header = utils.bits_to_header(recv_data)
        self.last_received_seq_num = fin_fin_header.seq_num
        if fin_fin_header.fin == 1:
          terminate_ack_header = utils.Header(self.next_seq_num, self.last_received_seq_num + 1, syn = 0, ack = 1, fin = 0)
          self.next_seq_num += 1
          send_udp(terminate_ack_header.bits())
          # self.update_state(States.TIME_WAIT)
          self.update_state(States.CLOSED)
      else:
        pass

  def update_state(self, new_state):
    if utils.DEBUG:
      print(self.client_state, '->', new_state)
    self.client_state = new_state

  def send_reliable_message(self, message):
    chunks=list(chunkstring(message, MSS)) # divide message into appropriate length segments
    length = len(chunks)
    chunk_count = 0
    while chunk_count < length:
      chunk=chunks[chunk_count]
      # construct header
      msg_header = utils.Header(self.next_seq_num, self.last_received_seq_num + 1, syn=0, ack=0, fin=0)
      # concatenate header with message
      full_msg = msg_header.bits() + chunk.encode()
      # keep track of the number of bytes sent so we can check the ack later
      bytes_sent=len(chunk)
      self.next_seq_num += bytes_sent
      # send to server
      send_udp(full_msg)



      # make sure we receive ack between each chunk
      try:
        recv_data, addr = sock.recvfrom(1024)
        ack_header = utils.bits_to_header(recv_data)
        
      except socket.timeout: # client has not received ACK so unsure if server got message. must resend message.
        # need invariant that seq num stays same, adding bytes_sent earlier, so seq num remains unchanged
        # if a resend is needed: with it as MSS, this caused an edge case for the last chunk that it could
        # be duplicated
        self.next_seq_num -= bytes_sent # was MSS
        print('resending from socket.timeout, no ack received in time')
        continue

      # we've received an ack. check the ack_num is as expected
      if ack_header.ack_num != self.next_seq_num: # server did not receive last message. must resend message.
        print('resending from mismatched ack expected %s', self.next_seq_num)
        self.next_seq_num -= bytes_sent # was MSS

        continue

      # increment last received seq_num from server
      self.last_received_seq_num = ack_header.seq_num
      # move on to next chunk
      chunk_count+=1


  # note: this implementation does NOT use receive_acks
  # to properly handle some aspects (pipelining, etc.), 
  # this likely would be necessary, but this implementation does 
  # stop-and-wait, so there is a guarantee on the correspondence 
  # of sends and receives with client and server waiting on each other
  def receive_acks_sub_process(self, lst_rec_ack_shared):
    while True:
      recv_data, addr = sock.recvfrom(1024)
      header = utils.bits_to_header(recv_data)
      if header.ack_num > lst_rec_ack_shared.value:
        lst_rec_ack_shared.value = header.ack_num

  def receive_acks(self):
    # Start receive_acks_sub_process as a process
    lst_rec_ack_shared = Value('i', self.last_received_ack)
    p = multiprocessing.Process(target=self.receive_acks_sub_process, args=(lst_rec_ack_shared,))
    p.start()
    # Wait for 1 seconds or until process finishes
    p.join(1)
    # If process is still active, we kill it
    if p.is_alive():
      p.terminate()
      p.join()
    self.last_received_ack = lst_rec_ack_shared.value

client = Client()

#client.send_reliable_message("This message is to be received in pieces")

#n_seg = 50
#x = [str(i) + ' ' for i in range(0,MSS * n_seg)]
#x_str = ''.join(x)
client.send_reliable_message("TESTING")

#client.send_reliable_message("It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness") #, it was the epoch of belief, it was the epoch of incredulity, it was the season of Light, it was the season of Darkness, it was the spring of hope, it was the winter of despair, we had everything before us, we had nothing before us, we were all going direct to Heaven, we were all going direct the other way---in short, the period was so far like the present period, that some of its noisiest authorities insisted on its being received, for good or for evil, in the superlative degree of comparison only. There were a king with a large jaw and a queen with a plain face, on the throne of England; there were a king with a large jaw and a queen with a fair face, on the throne of France. In both countries it was clearer than crystal to the lords of the State preserves of loaves and fishes, that things in general were settled for ever. It was the year of Our Lord one thousand seven hundred and seventy-five. Spiritual revelations were conceded to England at that favoured period, as at this. Mrs. Southcott had recently attained her five-and-twentieth blessed birthday, of whom a prophetic private in the Life Guards had heralded the sublime appearance by announcing that arrangements were made for the swallowing up of London and Westminster. Even the Cock-lane ghost had been laid only a round dozen of years, after rapping out its messages, as the spirits of this very year last past (supernaturally deficient in originality) rapped out theirs. Mere messages in the earthly order of events had lately come to the English Crown and People, from a congress of British subjects in America: which, strange to relate, have proved more important to the human race than any communications yet received through any of the chickens of the Cock-lane brood.")

#client.send_reliable_message("Bigger message to be received in pieces: Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin mattis erat vitae ipsum vehicula convallis. Aenean libero sapien, semper eget porta sit amet, tristique quis arcu. Cras bibendum vitae ex feugiat ultricies. Quisque tincidunt, eros eget lobortis consequat, justo nunc convallis ipsum, id feugiat dui lacus eget tortor. Donec pellentesque eros leo, volutpat elementum ex efficitur nec. Nunc mattis neque sed nisl viverra ullamcorper. Pellentesque dolor dolor, venenatis a imperdiet quis, sollicitudin nec erat. Maecenas nec risus id diam ultricies semper. Morbi ac odio faucibus, ornare lorem sed, vulputate nibh. Curabitur ut justo sapien. Aenean consectetur pharetra venenatis. Vestibulum neque tellus, sodales non semper at, pellentesque ac libero. Sed vitae mattis risus. Donec rutrum venenatis magna ac iaculis. Donec arcu augue, molestie eget imperdiet at, vulputate eu odio. Vivamus quis libero luctus, malesuada quam nec, elementum dui. Cras mattis lorem dolor. In facilisis dictum auctor. Integer lacinia metus libero, et accumsan nibh tristique vel. Sed cursus risus id faucibus accumsan. Donec porta sed tortor vitae interdum. Nullam pellentesque bibendum odio, ut gravida sem consequat et. Etiam dignissim dui nisl, in facilisis libero dapibus id. Suspendisse rhoncus turpis sit amet libero tempus luctus. Quisque laoreet viverra pellentesque. Praesent vel tincidunt massa. In ac ligula ac sapien bibendum pharetra. Maecenas in porta ipsum. Maecenas id accumsan eros, eu dictum odio. Pellentesque ac posuere neque. Cras egestas massa lorem, vitae porttitor ante auctor at. Donec consectetur a urna maximus commodo. Sed ac velit vel felis pharetra varius posuere et lacus. Fusce lobortis, ex sit amet interdum porta, ante nulla varius dolor, eget tempus sem diam eget sapien. Aliquam nec dapibus felis. Nunc dui lacus, interdum nec lacinia sit amet, rhoncus quis massa. Donec lectus dui, auctor nec nisl vitae, congue lobortis enim. Donec dignissim nunc vitae ullamcorper molestie. Phasellus tempor ultricies turpis laoreet pulvinar. Phasellus semper venenatis arcu, eget pharetra ipsum lobortis eu. Sed ornare sagittis ex vitae fermentum. Praesent vestibulum fringilla sapien nec sagittis. Suspendisse quis aliquam augue. Sed odio purus, tincidunt eu accumsan id, ullamcorper vitae felis.")

client.terminate()
