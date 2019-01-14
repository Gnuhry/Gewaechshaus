package com.example.win7.gartenhausapp_2;

import android.content.Intent;
import android.content.SharedPreferences;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {

    private SharedPreferences.Editor editor;
    public static String IP = "192.168.178.78";
    public static final int Port = 5000;
    public static Client client;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        startActivity(new Intent(getApplicationContext(), MainActivity.class));
        return super.onOptionsItemSelected(item);
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        SharedPreferences sharedPreferences = getSharedPreferences("Speicher", MODE_PRIVATE);
        editor = sharedPreferences.edit();
        IP = sharedPreferences.getString("IP", IP);
        client = new Client(IP, Port);
        ((TextView) findViewById(R.id.edTIP)).setText(IP);
        ((TextView) findViewById(R.id.edTIP)).addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {

            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {

            }

            @Override
            public void afterTextChanged(Editable editable) {
                char[] test = editable.toString().toCharArray();
                int count = 0;
                for (char Test_ : test)
                    if (Test_ == '.')
                        count++;

                if (count == 3 && test.length < 16)  //check for ip4 format
                {
                    IP = editable.toString();
                    client = new Client(IP, Port);
                    Toast.makeText(getApplicationContext(), "Neue IP: " + IP, Toast.LENGTH_SHORT).show();
                    editor.putString("IP", IP).apply();

                }
            }
        });

    }

    public void btnClick2(View view) {
        Intent intent = new Intent(this, Main2Activity.class);
        startActivity(intent);

    }

    public void btnClick3(View view) {
        Intent intent = new Intent(this, Main3Activity.class);
        startActivity(intent);
    }
}
